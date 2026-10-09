#include "allocators.hpp"
#include <iostream>
#include <type_traits>
void require(bool b) { if(!b) throw std::runtime_error("assertion failed"); }
template<class F> void fails(F action) { bool failed=false;try { action(); } catch(const std::exception&) { failed=true; }require(failed); }
int main() {
  using namespace allocators;
  static_assert(!std::is_copy_constructible_v<Pool>);
  static_assert(!std::is_move_constructible_v<Stack>);
  static_assert(!std::is_copy_constructible_v<Arena>);
  Pool pool;std::vector<void*> pointers;for(int i=0;i<512;++i) pointers.push_back(pool.allocate(128,64));
  fails([&]{pool.allocate(1);});for(int i=0;i<512;i+=2) pool.release(pointers[i]);
  require(pool.free_extent()==std::pair<std::size_t,std::size_t>{32768,128});
  fails([&]{pool.release(pointers[0]);});fails([&]{pool.release(static_cast<std::byte*>(pointers[1])+1);});
  fails([&]{pool.allocate(129);});
  Stack stack;auto a=stack.allocate(3),b=stack.allocate(64,64);require(reinterpret_cast<std::uintptr_t>(b)%64==0);
  fails([&]{stack.release(a);});stack.release(b);stack.release(a);require(stack.used()==0);
  fails([&]{stack.release(a);});fails([&]{stack.allocate(capacity+1);});
  Arena arena;
  for(std::size_t align:{1,2,4,8,16,32,64}) { auto p=arena.allocate(3,align); require(reinterpret_cast<std::uintptr_t>(p)%align==0); }
  fails([&]{arena.allocate(1,3);});fails([&]{arena.allocate(capacity);});arena.reset();require(arena.used()==0);
  arena.allocate(capacity,64);fails([&]{arena.allocate(1);});arena.reset();
  for(int i=0;i<1000;++i) { auto p=arena.allocate(64,64);require(reinterpret_cast<std::uintptr_t>(p)%64==0);arena.reset(); }
  Arena owned;auto first=owned.allocate(3);owned.reset();require(owned.allocate(3)==first);
  std::cout<<"pool, stack and Arena boundaries passed\n";
}
