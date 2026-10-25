#pragma once
#include <atomic>
#include <cstddef>
namespace engine_labs {
// Padding is an explicit experiment parameter, not an assumed platform fact.
// The S8 executable first verifies the observed cache line is exactly 64 bytes.
template<class T,bool Padded>
struct alignas(Padded ? 64 : alignof(std::atomic<T>)) AtomicCell:std::atomic<T> {
    using std::atomic<T>::atomic;
    AtomicCell():std::atomic<T>(0) {}
};
static_assert(sizeof(AtomicCell<std::size_t,false>)==sizeof(std::atomic<std::size_t>));
static_assert(alignof(AtomicCell<std::size_t,false>)==alignof(std::atomic<std::size_t>));
}
