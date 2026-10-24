// Tool-only: measure tight world mesh bounds on actual FBX instances, not a stub.
#include "ufbx.h"
#include "../../common/workload_io.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv) {
 try {
    if(argc!=5)throw std::runtime_error("usage: cost_collector input.fbx outdir ns_per_work_unit time_compression");
    const double ns_per_unit=std::stod(argv[3]), compression=std::stod(argv[4]);
    if(!(ns_per_unit>0 && compression>0))throw std::runtime_error("invalid calibration");
    ufbx_load_opts opts{};ufbx_error error{};
    auto scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>(ufbx_load_file(argv[1],&opts,&error),ufbx_free_scene);
    if(!scene)throw std::runtime_error("FBX load failed");
    std::filesystem::create_directories(argv[2]);
    std::ofstream csv(std::filesystem::path(argv[2])/"mesh-costs.csv");
    csv<<"mesh,instance,vertices,median_ns,work_units\n";
    std::map<std::uint32_t,std::size_t> counts;
    std::size_t mesh_id=0,instances=0;double sink=0;
    for(auto* mesh:scene->meshes) {
        std::size_t instance_id=0;
        for(auto* node:mesh->instances) {
            std::vector<double> samples;
            for(int repeat=0;repeat<7;++repeat) {
                std::array<double,6> b;
                for(int k=0;k<3;++k){b[k]=std::numeric_limits<double>::infinity();b[k+3]=-b[k];}
                const auto begin=Clock::now();
                for(auto v:mesh->vertices) {
                    auto p=ufbx_transform_position(&node->geometry_to_world,v);
                    const double xyz[]={p.x,p.y,p.z};
                    for(int k=0;k<3;++k){b[k]=std::min(b[k],xyz[k]);b[k+3]=std::max(b[k+3],xyz[k]);}
                }
                const auto end=Clock::now();
                for(double value:b)sink+=value;
                if(repeat>=2)samples.push_back(std::chrono::duration<double,std::nano>(end-begin).count());
            }
            std::sort(samples.begin(),samples.end());const double median=samples[samples.size()/2];
            const auto units=static_cast<std::uint32_t>(std::clamp(std::llround(median/(ns_per_unit*compression)),1LL,1000000000LL));
            ++counts[units];++instances;
            csv<<mesh_id<<','<<instance_id++<<','<<mesh->num_vertices<<','<<median<<','<<units<<'\n';
        }
        ++mesh_id;
    }
    if(!std::isfinite(sink))throw std::runtime_error("invalid mesh checksum");
    std::ofstream hist(std::filesystem::path(argv[2])/"histogram.json");
    hist<<"{\"unit\":\"work_units\",\"buckets\":[";bool first=true;
    for(auto [units,count]:counts){if(!first)hist<<',';first=false;hist<<'['<<units<<','<<std::uint64_t(units)+1<<','<<count<<']';}
    hist<<"]}\n";
    std::ofstream meta(std::filesystem::path(argv[2])/"collection.json");
    meta<<std::setprecision(17)<<"{\"source_sha256\":\""<<ecl::workload::sha256(ecl::workload::read_bytes(argv[1]))
        <<"\",\"ufbx_version\":\"0.23.1\",\"operation\":\"world_mesh_bounds_all_vertices\",\"warmup_per_mesh\":2,\"samples_per_mesh\":5,\"instances\":"
        <<instances<<",\"ns_per_work_unit\":"<<ns_per_unit<<",\"time_compression\":"<<compression<<",\"checksum\":"<<sink<<"}\n";
    std::cout<<instances<<" actual mesh-instance costs collected\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
