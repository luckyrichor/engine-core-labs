#pragma once
#include <cstddef>
#include <cstdint>
namespace engine_labs {
inline std::uint64_t workload(std::size_t task, std::size_t iterations) {
    std::uint64_t value = task + 1;
    for (std::size_t i=0; i<iterations; ++i) {
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
    }
    return value;
}
}
