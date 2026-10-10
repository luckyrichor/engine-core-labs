// Linux first-touch control versus growing live malloc set. Run modes in fresh processes.
#include "allocators.hpp"
#include <chrono>
#include <iostream>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
using Clock=std::chrono::steady_clock;
int main(int argc,char**argv){try{
 const std::string mode=argc==2?argv[1]:"first_touch";
 const std::size_t page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE)),bytes=65536,count=512;
 if(mode!="first_touch"&&mode!="prefaulted"&&mode!="malloc_growth")throw std::invalid_argument("mode");
 std::vector<void*> blocks;blocks.reserve(count);
 if(mode!="malloc_growth"){
  auto p=mmap(nullptr,bytes*count,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(p==MAP_FAILED)throw std::bad_alloc();
  for(std::size_t i=0;i<count;++i)blocks.push_back(static_cast<char*>(p)+i*bytes);
  if(mode=="prefaulted")for(std::size_t i=0;i<bytes*count;i+=page)*static_cast<volatile char*>(static_cast<void*>(static_cast<char*>(p)+i))=1;
 }
 std::cout<<"mode,index,bytes,allocate_and_touch_ns,minor_faults,major_faults\n";
 std::vector<std::string> lines;lines.reserve(count);
 for(std::size_t i=0;i<count;++i){
  rusage before{},after{};getrusage(RUSAGE_SELF,&before);auto start=Clock::now();
  void* p=mode=="malloc_growth"?std::malloc(bytes):blocks[i];if(!p)throw std::bad_alloc();
  for(std::size_t j=0;j<bytes;j+=page)static_cast<volatile char*>(p)[j]=1;
  auto end=Clock::now();getrusage(RUSAGE_SELF,&after);
  if(mode=="malloc_growth")blocks.push_back(p);
  lines.push_back(mode+","+std::to_string(i)+","+std::to_string(bytes)+","+std::to_string(std::chrono::duration<double,std::nano>(end-start).count())+","+std::to_string(after.ru_minflt-before.ru_minflt)+","+std::to_string(after.ru_majflt-before.ru_majflt));
 }
 for(auto p:blocks)for(std::size_t j=0;j<bytes;j+=page)if(static_cast<volatile char*>(p)[j]!=1)throw std::runtime_error("payload");
 for(const auto& line:lines)std::cout<<line<<'\n';
 if(mode=="malloc_growth")for(auto p:blocks)std::free(p);else if(munmap(blocks.front(),bytes*count)!=0)throw std::runtime_error("munmap");
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
