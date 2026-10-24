#pragma once
#include <cstdint>
namespace engine_labs::phase3 {
#ifdef _MSC_VER
__declspec(noinline)
#else
[[gnu::noinline]]
#endif
inline std::uint64_t work(std::uint64_t id,std::uint32_t cost) {
    std::uint64_t digest=id+1;
    for(std::uint32_t k=0;k<cost;++k)digest=(digest^(digest>>7))*0x9e3779b97f4a7c15ULL+k;
    return digest;
}
}
