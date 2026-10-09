#include "workload_shape.hpp"
#include <array>
#include <stdexcept>
int main(){
 using namespace engine_labs;
 auto adversarial=workload_shape(10000,500,"heterogeneous",0);
 if(adversarial.heavy!=79)throw std::runtime_error("heavy count");
 for(std::uint64_t seed=20261009;seed<20261019;++seed){
  auto a=workload_shape(10000,500,"heterogeneous_random",seed);
  auto b=workload_shape(10000,500,"heterogeneous_random",seed);
  if(a.iterations!=b.iterations||a.hash!=b.hash||a.heavy!=79)throw std::runtime_error("reproducibility");
  std::array<int,8> queues{};std::size_t heavy=0,total=0;
  for(std::size_t i=0;i<a.iterations.size();++i){auto cost=a.iterations[i];if(cost!=500&&cost!=50000)throw std::runtime_error("invalid cost");if(cost==50000){++heavy;++queues[i%8];}total+=cost;}
  if(heavy!=79||total!=500*(10000+99*79))throw std::runtime_error("work conservation");
  for(auto count:queues)if(count==0)throw std::runtime_error("fixture accidentally concentrated");
  if(a.iterations==adversarial.iterations)throw std::runtime_error("not random fixture");
 }
 bool refused=false;try{workload_shape(1,1000001,"heterogeneous_random",0);}catch(const std::invalid_argument&){refused=true;}
 if(!refused)throw std::runtime_error("overflow guard");
}
