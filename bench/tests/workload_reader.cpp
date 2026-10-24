#include "workload_io.hpp"
#include <iostream>
#include <numeric>
using namespace ecl::workload;
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: workload_reader meta.json");
        // Standard SHA256 test vectors exercise empty input and multi-block padding.
        require(sha256({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty");
        require(sha256({'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc");
        auto meta=verify_meta(argv[1]); const auto& kind=meta.at("kind").str();
        std::map<std::string,std::uint64_t> stats;
        if(kind!="constructive") {
            require(meta.at("files").object.size()==1,"expected one data file");
            auto data=verified_file(argv[1],meta,meta.at("files").object.begin()->first);
            if(kind=="task_costs") {
                auto costs=read_task_costs(data);stats["task_count"]=costs.size();
                stats["cost_sum"]=std::accumulate(costs.begin(),costs.end(),std::uint64_t{0});
            } else if(kind=="aabb_scene") {
                auto boxes=read_aabbs(data);std::uint64_t pairs=0;
                for(std::size_t i=0;i<boxes.size();++i)for(std::size_t j=0;j<i;++j) {
                    bool overlap=true;for(unsigned k=0;k<3;++k)if(boxes[i][k]>boxes[j][k+3]||boxes[j][k]>boxes[i][k+3])overlap=false;
                    if(overlap)++pairs;
                }
                stats={{"object_count",boxes.size()},{"overlap_pairs",pairs}};
            } else if(kind=="alloc_trace") {
                std::map<std::uint64_t,std::pair<std::uint64_t,std::uint32_t>> live;
                std::uint64_t total=0,previous=0;stats={{"allocations",0},{"frees",0},{"peak_live_bytes",0}};
                for(const auto& r:read_trace(data)) {
                    require(r.timestamp>=previous,"nonmonotonic trace");previous=r.timestamp;
                    if(r.operation==0) {
                        require(r.size>0&&live.emplace(r.object_id,std::make_pair(r.size,r.thread)).second,"duplicate allocation");
                        total+=r.size;++stats["allocations"];stats["peak_live_bytes"]=std::max(stats["peak_live_bytes"],total);
                    } else {
                        require(live.at(r.object_id)==std::make_pair(r.size,r.thread),"invalid free");
                        live.erase(r.object_id);total-=r.size;++stats["frees"];
                    }
                }require(live.empty(),"live allocations at end");
            } else if(kind=="resource_manifest") {
                auto resources=read_resources(data);std::uint64_t total=0;
                for(const auto& r:resources)total+=r.bytes;
                stats={{"file_count",resources.size()},{"total_bytes",total}};
            } else throw std::runtime_error("unknown kind");
        }
        require(stats.size()==meta.at("answer").object.size(),"answer fields mismatch");
        for(const auto& [key,value]:stats)require(value==meta.at("answer").at(key).u64(),"independent answer mismatch");
        std::cout<<"{";bool first=true;
        for(const auto& [key,value]:stats) {if(!first)std::cout<<",";first=false;std::cout<<"\""<<key<<"\":"<<value;}
        std::cout<<"}\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
