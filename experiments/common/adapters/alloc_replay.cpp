#include "../alloc_adapter.hpp"
#include "../workload_io.hpp"
#include <iostream>
#include <limits>
#include <map>
#include <memory>
namespace {
struct Live { void* pointer; std::uint64_t size; std::uint32_t thread; };
std::uint8_t pattern(std::uint64_t id, std::uint64_t offset) {
    return static_cast<std::uint8_t>((id * 131) ^ (offset * 17) ^ (offset >> 8));
}
}
int main(int argc, char** argv) {
    using namespace ecl::workload;
    auto allocator=std::make_unique<ecl::adapter::Allocator>();
    std::map<std::uint64_t,Live> live;
    try {
        require(argc==2, "usage: alloc_replay meta.json");
        const auto meta=verify_meta(argv[1]);
        require(meta.at("kind").str()=="alloc_trace", "expected alloc_trace");
        require(meta.at("files").object.size()==1, "expected one trace file");
        const auto records=read_trace(verified_file(argv[1],meta,meta.at("files").object.begin()->first));
        const auto identity=allocator->identity();
        std::uint64_t previous=0, bytes=0, peak=0, allocations=0, frees=0;
        for (const auto& record:records) {
            require(record.timestamp>=previous, "nonmonotonic trace"); previous=record.timestamp;
            if (record.operation==0) {
                require(record.size>0 && record.size<=std::numeric_limits<std::size_t>::max(), "invalid allocation size");
                require(!live.contains(record.object_id), "duplicate allocation");
                require(bytes<=std::numeric_limits<std::uint64_t>::max()-record.size, "live size overflow");
                void* pointer=allocator->allocate(static_cast<std::size_t>(record.size));
                if (!pointer) throw std::bad_alloc();
                try { live.emplace(record.object_id,Live{pointer,record.size,record.thread}); }
                catch (...) { allocator->release(pointer); throw; }
                auto* data=static_cast<std::uint8_t*>(pointer);
                for(std::uint64_t i=0;i<record.size;++i) data[i]=pattern(record.object_id,i);
                bytes+=record.size; peak=std::max(peak,bytes); ++allocations;
            } else {
                require(record.operation==1, "invalid operation");
                const auto found=live.find(record.object_id);
                require(found!=live.end(), "free without live allocation");
                const auto item=found->second;
                require(item.size==record.size && item.thread==record.thread, "free metadata mismatch");
                const auto* data=static_cast<const std::uint8_t*>(item.pointer);
                for(std::uint64_t i=0;i<item.size;++i) require(data[i]==pattern(record.object_id,i), "allocation byte pattern corrupted");
                allocator->release(item.pointer); live.erase(found); bytes-=item.size; ++frees;
            }
        }
        require(live.empty() && bytes==0, "live allocations at end");
        const auto& answer=meta.at("answer");
        require(allocations==answer.at("allocations").u64(), "allocation count mismatch");
        require(frees==answer.at("frees").u64(), "free count mismatch");
        require(peak==answer.at("peak_live_bytes").u64(), "peak live bytes mismatch");
        std::cerr << identity << '\n';
        std::cout << "{\"allocator\":\"" << ECL_ALLOC_NAME << "\",\"identity_verified\":true,\"allocations\":"
                  << allocations << ",\"frees\":" << frees << ",\"peak_live_bytes\":" << peak << ",\"all_released\":true}\n";
    } catch(const std::exception& error) {
        for(const auto& [id,item]:live) { (void)id; allocator->release(item.pointer); }
        std::cerr << error.what() << '\n'; return 1;
    }
}
