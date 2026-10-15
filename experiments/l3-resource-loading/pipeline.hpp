#pragma once
#include "job_system.hpp"
#include "../l2-allocators/allocators.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <cstdint>
namespace resources {
struct Asset {std::size_t id;std::uint64_t hash;};
// Fixed 64 KiB slots backed by L2 Arenas. Lease lifetime ends after decode;
// the installed Asset owns only its hash, never a pointer into scratch memory.
class Buffers {
 std::mutex mutex;
 std::vector<std::unique_ptr<allocators::Arena>> slots;
 std::vector<std::size_t> free;
public:
 explicit Buffers(std::size_t count){
  if(!count)throw std::invalid_argument("buffer capacity");
  free.reserve(count);
  for(std::size_t i=0;i<count;++i){slots.push_back(std::make_unique<allocators::Arena>());free.push_back(i);}
 }
 struct Lease {
  Buffers& owner;std::size_t index;unsigned char* data;
  Lease(const Lease&)=delete;
  Lease(Buffers& o,std::size_t i,unsigned char* p):owner(o),index(i),data(p){}
  ~Lease(){std::lock_guard lock(owner.mutex);owner.slots[index]->reset();owner.free.push_back(index);}
 };
 Lease acquire(){
  std::lock_guard lock(mutex);
  if(free.empty())throw std::runtime_error("buffer capacity exhausted");
  auto i=free.back();
  auto* p=static_cast<unsigned char*>(slots[i]->allocate(65536));free.pop_back();return Lease(*this,i,p);
 }
};
inline Asset decode(const std::filesystem::path& path,std::size_t id,std::size_t iterations,unsigned char* bytes){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("asset open failed");
 file.read(reinterpret_cast<char*>(bytes),65536);
 if(file.gcount()!=65536 || file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("asset size invalid");
 std::uint64_t hash=14695981039346656037ULL;
 for(std::size_t i=0;i<65536;++i){hash^=bytes[i];hash*=1099511628211ULL;}
 for(std::size_t i=0;i<iterations;++i){hash^=hash>>12;hash^=hash<<25;hash^=hash>>27;hash*=2685821657736338717ULL;}
 return {id,hash};
}
inline Asset load(const std::filesystem::path& path,std::size_t id,std::size_t iterations){
 std::vector<unsigned char> bytes(65536);
 return decode(path,id,iterations,bytes.data());
}

class Pipeline {
 using Clock=std::chrono::steady_clock;
 std::shared_ptr<Buffers> buffers;
 std::map<std::size_t,Clock::time_point> installed;
 engine_labs::JobSystem pool;
 std::map<std::size_t,std::future<Asset>> pending;
 std::map<std::size_t,Asset> ready;
 std::size_t capacity,iterations;
public:
 Pipeline(std::size_t workers,std::size_t limit,std::size_t decode,bool reuse=false):buffers(reuse?std::make_shared<Buffers>(limit):nullptr),pool(workers,true),capacity(limit),iterations(decode){if(!limit)throw std::invalid_argument("capacity");}
 bool request(const std::filesystem::path& path,std::size_t id){
  if(ready.contains(id)||pending.contains(id))return true;
  if(pending.size()>=capacity)return false;
  auto task=std::make_shared<std::packaged_task<Asset()>>([path,id,iterations=iterations,scratch=buffers]{
   if(!scratch)return load(path,id,iterations);
   auto lease=scratch->acquire();return decode(path,id,iterations,lease.data);
  });
  auto future=task->get_future();pool.submit([task]{(*task)();});
  pending.emplace(id,std::move(future));return true;
 }
 std::size_t commit(std::size_t budget){
  std::size_t committed=0;
  for(auto it=pending.begin();it!=pending.end()&&committed<budget;){
   if(it->second.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++it;continue;}
   // Exceptions propagate to the owner; no half-installed failed asset.
   auto future=std::move(it->second);it=pending.erase(it);
   auto asset=future.get();ready.emplace(asset.id,asset);installed.emplace(asset.id,Clock::now());++committed;
  }return committed;
 }
 Clock::time_point installed_at(std::size_t id)const{return installed.at(id);}
 bool contains(std::size_t id)const{return ready.contains(id);}
 std::size_t in_flight()const{return pending.size();}
 std::uint64_t checksum()const{std::uint64_t n=0;for(auto&[id,a]:ready){(void)id;n+=a.hash;}return n;}
 void drain(){while(!pending.empty()){commit(capacity);std::this_thread::yield();}}
};
}
