#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
namespace allocators {
constexpr std::size_t capacity=65536;
class Pool {
  alignas(64) std::array<std::byte,capacity> storage{};
  std::array<bool,capacity/128> live{};
  std::size_t head{};
  static constexpr std::size_t end=capacity/128;
public:
  Pool() {for(std::size_t i=0;i<end;++i){auto next=i+1;std::memcpy(storage.data()+i*128,&next,sizeof(next));}}
  Pool(const Pool&)=delete;
  Pool& operator=(const Pool&)=delete;
  void* allocate(std::size_t size,std::size_t alignment=16) {
    if(size==0 || size>128 || alignment==0 || alignment>64 || (alignment&(alignment-1))) throw std::invalid_argument("invalid allocation");
    if(head==end) throw std::bad_alloc();
    auto index=head;std::memcpy(&head,storage.data()+index*128,sizeof(head));live[index]=true;return storage.data()+index*128;
  }
  void release(void* pointer) {
    auto address=reinterpret_cast<std::uintptr_t>(pointer),base=reinterpret_cast<std::uintptr_t>(storage.data());
    if(address<base || address>=base+capacity || (address-base)%128) throw std::invalid_argument("foreign pointer");
    auto index=(address-base)/128;if(!live[index]) throw std::invalid_argument("double release");
    live[index]=false;std::memcpy(storage.data()+index*128,&head,sizeof(head));head=index;
  }
  std::size_t metadata_bytes()const{return sizeof(*this)-capacity;}
  std::pair<std::size_t,std::size_t> free_extent() const {
    std::size_t run=0,best=0,count=0;for(bool used:live) { if(used) run=0;else { ++count;best=std::max(best,++run); } }return {count*128,best*128};
  }
};
class Stack {
  alignas(64) std::array<std::byte,capacity> storage{};
  std::size_t offset{};
  std::vector<std::pair<void*,std::size_t>> frames;
public:
  Stack() { frames.reserve(4096); }
  Stack(const Stack&)=delete;
  Stack& operator=(const Stack&)=delete;
  void* allocate(std::size_t size,std::size_t alignment=16) {
    if(size==0 || alignment==0 || alignment>64 || (alignment&(alignment-1))) throw std::invalid_argument("invalid allocation");
    auto remaining=capacity-offset;void* pointer=storage.data()+offset;
    if(!std::align(alignment,size,pointer,remaining)) throw std::bad_alloc();
    frames.emplace_back(pointer,offset);offset=static_cast<std::byte*>(pointer)-storage.data()+size;return pointer;
  }
  void release(void* pointer) {
    if(frames.empty() || frames.back().first!=pointer) throw std::invalid_argument("not top of stack");
    offset=frames.back().second;frames.pop_back();
  }
  void reset() { offset=0;frames.clear(); }
  std::size_t used() const { return offset; }
  std::size_t metadata_bytes()const{return sizeof(*this)-capacity+frames.capacity()*sizeof(frames[0]);}
};
class Arena {
  alignas(64) std::array<std::byte,capacity> storage{};
  std::size_t offset{};
public:
  Arena()=default;
  Arena(const Arena&)=delete;
  Arena& operator=(const Arena&)=delete;
  void* allocate(std::size_t size,std::size_t alignment=16) {
    if(size==0 || alignment==0 || alignment>64 || (alignment&(alignment-1)))throw std::invalid_argument("invalid allocation");
    auto remaining=capacity-offset;void* pointer=storage.data()+offset;
    if(!std::align(alignment,size,pointer,remaining))throw std::bad_alloc();
    offset=static_cast<std::byte*>(pointer)-storage.data()+size;return pointer;
  }
  void reset(){offset=0;}
  std::size_t used()const{return offset;}
  std::size_t metadata_bytes()const{return sizeof(*this)-capacity;}
};
}
