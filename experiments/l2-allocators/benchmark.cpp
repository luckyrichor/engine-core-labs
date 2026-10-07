#include "reference_arena.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
using Clock=std::chrono::steady_clock;
int main() {
  using namespace allocators;
  std::cout<<"allocator,repeat,count,p50_ns,p95_ns,p99_ns,reserved_payload_waste_bytes,external_fragmentation\n";
  std::uintptr_t checksum=0;
  for(int repeat=0;repeat<10;++repeat) for(int mode=0;mode<3;++mode) {
    Pool pool;Stack stack;CodexReferenceArena arena;std::vector<long> times;std::vector<void*> pointers;times.reserve(256);pointers.reserve(256);
    std::size_t payload=0;
    for(int i=0;i<256;++i) {
      std::size_t size=32*(1+i%3);payload+=size;auto start=Clock::now();
      void* p=mode==0?pool.allocate(size):mode==1?stack.allocate(size):arena.allocate(size);
      auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
      static_cast<std::byte*>(p)[0]=std::byte(i%256);checksum^=reinterpret_cast<std::uintptr_t>(p);pointers.push_back(p);times.push_back(elapsed);
    }
    std::size_t used=mode==0?256*128:mode==1?stack.used():arena.used();double fragmentation=0;
    if(mode==0) { for(int i=0;i<256;i+=2) pool.release(pointers[i]);auto [free,largest]=pool.free_extent();fragmentation=1.0-static_cast<double>(largest)/free; }
    else if(mode==1) { for(auto i=pointers.rbegin();i!=pointers.rend();++i) stack.release(*i); }
    else arena.reset();
    std::sort(times.begin(),times.end());
    std::cout<<(mode==0?"pool":mode==1?"stack":"codex_reference_arena")<<','<<repeat<<",256,"<<times[127]<<','<<times[243]<<','<<times[253]<<','<<used-payload<<','<<fragmentation<<'\n';
  }
  std::cerr<<"checksum="<<checksum<<'\n';
}
