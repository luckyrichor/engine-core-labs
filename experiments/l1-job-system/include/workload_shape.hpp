#pragma once
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
namespace engine_labs {
inline bool cost_skew(const std::string& d){return d=="heterogeneous" || d=="heterogeneous_random";}
struct WorkloadShape {
 std::vector<std::size_t> iterations;
 std::size_t heavy{};
 std::uint64_t hash=14695981039346656037ULL;
};
inline WorkloadShape workload_shape(std::size_t tasks,std::size_t base,const std::string& d,std::uint64_t seed){
 if(d!="balanced"&&d!="skewed"&&!cost_skew(d))throw std::invalid_argument("distribution");
 if(!tasks||tasks>10000000||!base||base>(cost_skew(d)?1000000:100000000))throw std::invalid_argument("workload bounds");
 WorkloadShape shape;shape.iterations.assign(tasks,base);
 if(d=="heterogeneous")for(std::size_t i=0;i<tasks;i+=128){shape.iterations[i]=base*100;++shape.heavy;}
 if(d=="heterogeneous_random"){
  // Partial Fisher-Yates: exact same heavy count, sampled without replacement.
  // SplitMix64 + rejection sampling avoids library-specific distribution code.
  std::vector<std::size_t> ids(tasks);std::iota(ids.begin(),ids.end(),0);
  auto next=[&]{seed+=0x9e3779b97f4a7c15ULL;auto z=seed;z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);};
  shape.heavy=(tasks+127)/128;
  for(std::size_t k=0;k<shape.heavy;++k){std::uint64_t bound=tasks-k,threshold=(0-bound)%bound,r;do{r=next();}while(r<threshold);auto index=k+r%bound;std::swap(ids[k],ids[index]);shape.iterations[ids[k]]=base*100;}
 }
 for(auto count:shape.iterations){shape.hash^=(count!=base);shape.hash*=1099511628211ULL;}
 return shape;
}
}
