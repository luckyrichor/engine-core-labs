#include "allocators.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
using Clock=std::chrono::steady_clock;
constexpr std::size_t window=256, windows=400;
static std::ofstream window_file;
struct Malloc {void* allocate(std::size_t n){auto p=std::malloc(n);if(!p)throw std::bad_alloc();return p;}};
struct Control { unsigned char value{}; void* allocate(std::size_t){return &value;} };
template<class A,class Release>
void sample(const char* name,const char* pattern,int repeat,A& a,Release release){
 std::array<void*,window> pointers{};
 std::array<std::size_t,window> sizes{};
 for(std::size_t i=0;i<window;++i)sizes[i]=std::string(pattern)=="odd"?std::array<std::size_t,5>{3,17,33,65,95}[i%5]:32*(1+i%3);
 for(int warm=0;warm<50;++warm){for(std::size_t i=0;i<window;++i){pointers[i]=a.allocate(sizes[i]);*static_cast<volatile unsigned char*>(pointers[i])=static_cast<unsigned char>(i);}release(pointers);}
 std::vector<double> times;times.reserve(window*windows);std::vector<double> maxima;maxima.reserve(windows);
 std::uint64_t checksum=0;
 for(std::size_t w=0;w<windows;++w){
  double maximum=0;
  for(std::size_t i=0;i<window;++i){
   auto start=Clock::now();pointers[i]=a.allocate(sizes[i]);*static_cast<volatile unsigned char*>(pointers[i])=static_cast<unsigned char>(i);auto end=Clock::now();
   double ns=std::chrono::duration<double,std::nano>(end-start).count();times.push_back(ns);maximum=std::max(maximum,ns);
   auto value=*static_cast<volatile unsigned char*>(pointers[i]);if(value!=static_cast<unsigned char>(i))throw std::runtime_error("payload corruption");checksum+=value;
  }
  // Control uses one byte repeatedly; real allocators must preserve all blocks.
  if(std::string(name)!="clock_write_control")for(std::size_t i=0;i<window;++i)if(*static_cast<volatile unsigned char*>(pointers[i])!=static_cast<unsigned char>(i))throw std::runtime_error("overlapping allocations");
  maxima.push_back(maximum);if(window_file.is_open())window_file<<name<<','<<pattern<<','<<repeat<<','<<w<<','<<maximum<<'\n';release(pointers);
 }
 if(checksum!=windows*32640)throw std::runtime_error("tail checksum mismatch");
 std::sort(times.begin(),times.end());std::sort(maxima.begin(),maxima.end());
 auto q=[](const auto& v,double f){return v[static_cast<std::size_t>(f*(v.size()-1))];};
 std::cout<<name<<','<<pattern<<','<<repeat<<','<<times.size()<<','<<window<<','<<windows<<','<<q(times,.5)<<','<<q(times,.99)<<','<<q(times,.999)<<','<<times.back()<<','<<q(maxima,.99)<<','<<checksum<<'\n';
}
int main(int argc,char** argv){
 try{
  if(argc>2)throw std::invalid_argument("usage: allocator_tail [window_csv]");
  if(argc==2){window_file.open(argv[1]);if(!window_file)throw std::runtime_error("cannot open window CSV");window_file<<"allocator,pattern,repeat,window_index,max_ns\n";}
  std::cout<<"allocator,pattern,repeat,samples,window,windows,instrumented_p50_ns,instrumented_p99_ns,instrumented_p999_ns,instrumented_max_ns,window_max_p99_ns,checksum\n";
  for(int repeat=0;repeat<10;++repeat)for(const char* pattern:{"aligned","odd"})for(int k=0;k<5;++k){
   int mode=(repeat+k)%5;
   if(mode==0){allocators::Pool a;sample("pool",pattern,repeat,a,[&](const auto& p){for(auto it=p.rbegin();it!=p.rend();++it)a.release(*it);});}
   else if(mode==1){allocators::Stack a;sample("stack",pattern,repeat,a,[&](const auto& p){for(auto it=p.rbegin();it!=p.rend();++it)a.release(*it);});}
   else if(mode==2){allocators::Arena a;sample("arena",pattern,repeat,a,[&](const auto&){a.reset();});}
   else if(mode==3){Malloc a;sample("malloc",pattern,repeat,a,[](const auto& p){for(auto pointer:p)std::free(pointer);});}
   else{Control a;sample("clock_write_control",pattern,repeat,a,[](const auto&){});}
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
