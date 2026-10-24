#pragma once
#include "../l2-allocators/variable_heap.hpp"
#include <cstdlib>
#include <string>
namespace ecl::adapter {
// Each replay executable links exactly one malloc replacement. The local heap
// is explicit and deliberately does not interpose process malloc.
class Allocator {
#ifdef ECL_ALLOC_LOCAL
    allocators::VariableHeap heap;
#endif
public:
    void* allocate(std::size_t size) {
#ifdef ECL_ALLOC_LOCAL
        return heap.allocate(size);
#else
        return std::malloc(size);
#endif
    }
    void release(void* pointer) {
#ifdef ECL_ALLOC_LOCAL
        heap.release(pointer);
#else
        std::free(pointer);
#endif
    }
    std::string identity() const;
};
}
