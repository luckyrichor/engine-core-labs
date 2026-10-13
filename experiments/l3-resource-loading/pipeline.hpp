#pragma once
#include "job_system.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <cstdint>
namespace resources {
struct Asset {std::size_t id;std::uint64_t hash;};
inline Asset load(const std::filesystem::path& path,std::size_t id,std::size_t iterations){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("asset open failed");
 std::vector<unsigned char> bytes(65536);
 file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
 if(file.gcount()!=65536 || file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("asset size invalid");
 std::uint64_t hash=14695981039346656037ULL;
 for(auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}
 // Synthetic CPU decode; I/O is real, this is not an image or mesh codec.
 for(std::size_t i=0;i<iterations;++i){hash^=hash>>12;hash^=hash<<25;hash^=hash>>27;hash*=2685821657736338717ULL;}
 return {id,hash};
}
class Pipeline {
 engine_labs::JobSystem pool;
 std::map<std::size_t,std::future<Asset>> pending;
 std::map<std::size_t,Asset> ready;
 std::size_t capacity,iterations;
public:
 Pipeline(std::size_t workers,std::size_t limit,std::size_t decode):pool(workers,true),capacity(limit),iterations(decode){if(!limit)throw std::invalid_argument("capacity");}
 bool request(const std::filesystem::path& path,std::size_t id){
  if(ready.contains(id)||pending.contains(id))return true;
  if(pending.size()>=capacity)return false;
  auto task=std::make_shared<std::packaged_task<Asset()>>([path,id,decode=iterations]{return load(path,id,decode);});
  auto future=task->get_future();pool.submit([task]{(*task)();});
  pending.emplace(id,std::move(future));return true;
 }
 std::size_t commit(std::size_t budget){
  std::size_t committed=0;
  for(auto it=pending.begin();it!=pending.end()&&committed<budget;){
   if(it->second.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++it;continue;}
   // Exceptions propagate to the owner; no half-installed failed asset.
   auto future=std::move(it->second);it=pending.erase(it);
   auto asset=future.get();ready.emplace(asset.id,asset);++committed;
  }return committed;
 }
 bool contains(std::size_t id)const{return ready.contains(id);}
 std::size_t in_flight()const{return pending.size();}
 std::uint64_t checksum()const{std::uint64_t n=0;for(auto&[id,a]:ready){(void)id;n+=a.hash;}return n;}
 void drain(){while(!pending.empty()){commit(capacity);std::this_thread::yield();}}
};
}
