#include "allocators.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
using Clock=std::chrono::steady_clock;
constexpr std::size_t batch=256;
struct Malloc {
 void* allocate(std::size_t size){auto p=std::malloc(size);if(!p)throw std::bad_alloc();return p;}
};
template<class A,class Release,class Used>
void measure(const char* name,const char* pattern,int repeat,A& allocator,Release release,Used used,std::size_t metadata){
 std::array<std::size_t,batch> sizes{};std::array<void*,batch> pointers{};
 std::size_t payload=0;for(std::size_t i=0;i<batch;++i){sizes[i]=std::string(pattern)=="odd"?std::array<std::size_t,5>{3,17,33,65,95}[i%5]:32*(1+i%3);payload+=sizes[i];}
 std::vector<double> alloc_times,free_times;std::uint64_t checksum=0;std::size_t batches=0,reserved=0;
 for(int warm=0;warm<50;++warm){for(std::size_t i=0;i<batch;++i){pointers[i]=allocator.allocate(sizes[i]);*static_cast<volatile unsigned char*>(pointers[i])=static_cast<unsigned char>(i);}release(pointers);}
 auto begin=Clock::now();
 do{
  auto start=Clock::now();
  for(std::size_t i=0;i<batch;++i){pointers[i]=allocator.allocate(sizes[i]);*static_cast<volatile unsigned char*>(pointers[i])=static_cast<unsigned char>(i);}
  auto end=Clock::now();
  alloc_times.push_back(std::chrono::duration<double,std::nano>(end-start).count()/batch);
  for(std::size_t i=0;i<batch;++i){auto v=*static_cast<volatile unsigned char*>(pointers[i]);if(v!=i)throw std::runtime_error("payload overlap/corruption");checksum+=v;}
  reserved=used();
  auto freed=Clock::now();release(pointers);
  free_times.push_back(std::chrono::duration<double,std::nano>(Clock::now()-freed).count()/batch);
  ++batches;
 }while(batches<2000 || Clock::now()-begin<std::chrono::milliseconds(100));
 if(checksum!=batches*32640)throw std::runtime_error("checksum mismatch");
 std::sort(alloc_times.begin(),alloc_times.end());std::sort(free_times.begin(),free_times.end());
 auto q=[](const auto& values,double fraction){return values[static_cast<std::size_t>(fraction*(values.size()-1))];};
 std::cout<<name<<','<<pattern<<','<<repeat<<','<<batch<<','<<batches<<','<<batches*batch<<','<<q(alloc_times,.5)<<','<<q(alloc_times,.95)<<','<<q(alloc_times,.99)<<',' ;
 if(std::string(name)!="arena")std::cout<<q(free_times,.5);
 std::cout<<','<<q(free_times,.5)*batch<<','<<(std::string(name)=="arena"?1:batch)<<','<<(std::string(name)=="arena"?"unresolved_single_reset":"batch_mean")<<','<<payload<<',';
 if(std::string(name)!="malloc")std::cout<<reserved-payload;
 std::cout<<',';if(std::string(name)!="malloc")std::cout<<metadata;std::cout<<','<<checksum<<'\n';
}
int main(){
 try{
  std::cout<<"allocator,pattern,repeat,batch,batches,allocations,alloc_batch_p50_ns_per_op,alloc_batch_p95_ns_per_op,alloc_batch_p99_ns_per_op,reclaim_batch_p50_ns_per_op,reclaim_region_p50_ns,reclaim_operations,reclaim_interpretation,payload_bytes,internal_waste_bytes,bookkeeping_reserved_bytes,checksum\n";
  // Rotated order across repeats; object construction/reserves outside timing.
  for(int repeat=0;repeat<10;++repeat)for(const char* pattern:{"aligned","odd"})for(int offset=0;offset<4;++offset){
   int mode=(repeat+offset)%4;
   if(mode==0){allocators::Pool a;measure("pool",pattern,repeat,a,[&](const auto& p){for(auto it=p.rbegin();it!=p.rend();++it)a.release(*it);},[]{return batch*128;},a.metadata_bytes());}
   else if(mode==1){allocators::Stack a;measure("stack",pattern,repeat,a,[&](const auto& p){for(auto it=p.rbegin();it!=p.rend();++it)a.release(*it);},[&]{return a.used();},a.metadata_bytes());}
   else if(mode==2){allocators::Arena a;measure("arena",pattern,repeat,a,[&](const auto&){a.reset();},[&]{return a.used();},a.metadata_bytes());}
   else {Malloc a;measure("malloc",pattern,repeat,a,[](const auto& p){for(void* pointer:p)std::free(pointer);},[]{return 0;},0);}
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
