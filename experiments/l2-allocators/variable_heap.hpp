#pragma once
#include "allocators.hpp"
#include <map>
namespace allocators {
// First-fit variable-size extent heap for a fragmentation demonstration, not a
// production malloc replacement. Maps allocate bookkeeping outside the region.
class VariableHeap {
 alignas(64) std::array<std::byte,capacity> storage{};
 std::map<std::size_t,std::size_t> free_{{0,capacity}},live_;
public:
 VariableHeap()=default;
 VariableHeap(const VariableHeap&)=delete;
 VariableHeap& operator=(const VariableHeap&)=delete;
 void* allocate(std::size_t size){
  if(!size)throw std::invalid_argument("zero allocation");
  for(auto it=free_.begin();it!=free_.end();++it)if(it->second>=size){
   auto offset=it->first,remaining=it->second-size;
   live_.emplace(offset,size);
   try {if(remaining)free_.emplace(offset+size,remaining);}
   catch(...) {live_.erase(offset);throw;}
   free_.erase(it);return storage.data()+offset;
  }
  throw std::bad_alloc();
 }
 void release(void* pointer){
  auto address=reinterpret_cast<std::uintptr_t>(pointer),base=reinterpret_cast<std::uintptr_t>(storage.data());
  if(address<base||address>=base+capacity)throw std::invalid_argument("foreign pointer");
  auto found=live_.find(address-base);if(found==live_.end())throw std::invalid_argument("not a live start");
  auto offset=found->first,size=found->second;
  auto it=free_.emplace(offset,size).first;live_.erase(found);
  if(it!=free_.begin()){auto before=std::prev(it);if(before->first+before->second==it->first){before->second+=it->second;free_.erase(it);it=before;}}
  auto next=std::next(it);if(next!=free_.end()&&it->first+it->second==next->first){it->second+=next->second;free_.erase(next);}
 }
 std::pair<std::size_t,std::size_t> free_extent()const{
  std::size_t total=0,largest=0;for(auto [offset,size]:free_){(void)offset;total+=size;largest=std::max(largest,size);}return{total,largest};
 }
};
}
