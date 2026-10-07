#pragma once
#include "allocators.hpp"
// Codex, 2026-10-07: experimental reference to unblock validation under the
// user's temporary-implementation authorization. Not a completed learning task.
namespace allocators {
class CodexReferenceArena {
  alignas(64) std::array<std::byte,capacity> storage{};
  std::size_t offset{};
public:
  CodexReferenceArena()=default;
  CodexReferenceArena(const CodexReferenceArena&)=delete;
  CodexReferenceArena& operator=(const CodexReferenceArena&)=delete;
  void* allocate(std::size_t size,std::size_t alignment=16) {
    if(size==0 || alignment==0 || alignment>64 || (alignment&(alignment-1))) throw std::invalid_argument("invalid allocation");
    auto remaining=capacity-offset;void* pointer=storage.data()+offset;
    if(!std::align(alignment,size,pointer,remaining)) throw std::bad_alloc();
    offset=static_cast<std::byte*>(pointer)-storage.data()+size;return pointer;
  }
  void reset() { offset=0; }
  std::size_t used() const { return offset; }
};
}
