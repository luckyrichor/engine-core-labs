#include "job_system.hpp"
#include "workload.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <numeric>

// Counts remain round-robin balanced; every 128th task costs 100x.
// The periodic placement deliberately concentrates expensive tasks on queue 0
// for the measured power-of-two worker counts. This is a cost-skew fixture.
static std::size_t task_iterations(std::size_t id,std::size_t iterations,const std::string& distribution){
 if(distribution=="heterogeneous" && id%128==0)return iterations*100;
 return iterations;
}
int main(int argc,char** argv){
 try{
  if((argc==4 || argc==5) && std::string(argv[1])=="reference"){
   auto tasks=std::stoull(argv[2]),iterations=std::stoull(argv[3]);std::uint64_t expected=0;
   std::string distribution=argc==5?argv[4]:"balanced";
   if(distribution!="balanced"&&distribution!="skewed"&&distribution!="heterogeneous")throw std::invalid_argument("distribution");
   if(distribution=="heterogeneous"&&iterations>1000000)throw std::invalid_argument("heterogeneous iterations max 1000000");
   if(!tasks||tasks>10000000||!iterations||iterations>100000000)throw std::invalid_argument("workload bounds");
   for(std::size_t i=0;i<tasks;++i)expected+=engine_labs::workload(i,task_iterations(i,iterations,distribution));
   std::cout<<expected<<'\n';return 0;
  }
  if(argc!=6 && argc!=7)throw std::invalid_argument("usage: job_benchmark baseline|stealing THREADS TASKS ITERATIONS balanced|skewed|heterogeneous [prequeued|end_to_end]");
  std::string mode=argv[1],distribution=argv[5],phase=argc==7?argv[6]:"end_to_end";
  if(mode!="baseline"&&mode!="stealing")throw std::invalid_argument("mode");
  if(distribution!="balanced"&&distribution!="skewed"&&distribution!="heterogeneous")throw std::invalid_argument("distribution");
  if(phase!="prequeued"&&phase!="end_to_end")throw std::invalid_argument("phase");
  auto threads=std::stoull(argv[2]),tasks=std::stoull(argv[3]),iterations=std::stoull(argv[4]);
  if(!tasks||tasks>10000000||!iterations||iterations>100000000)throw std::invalid_argument("workload bounds");
  if(distribution=="heterogeneous"&&iterations>1000000)throw std::invalid_argument("heterogeneous iterations max 1000000");
  std::vector<std::atomic<unsigned>> seen(tasks);
  std::vector<std::atomic<std::uint64_t>> values(tasks);
  // Full-size untimed warmup on the same pool, including all worker queues.
  engine_labs::JobSystem pool(threads,mode=="stealing");
  {
   auto& warm=pool;
   for(std::size_t i=0;i<tasks;++i)warm.submit([i,iterations,&distribution]{static thread_local volatile std::uint64_t sink; sink=engine_labs::workload(i,task_iterations(i,iterations,distribution));(void)sink;},distribution=="skewed"?std::optional<std::size_t>(0):(distribution=="heterogeneous"?std::optional<std::size_t>(i%threads):std::nullopt));
   warm.wait();
  }
  if(phase=="prequeued")pool.pause();
  std::vector<std::future<void>> futures;futures.reserve(tasks);
  auto started=std::chrono::steady_clock::now();
  const auto enqueue_started=started;
  for(std::size_t i=0;i<tasks;++i)futures.push_back(pool.submit([&,i]{values[i].store(engine_labs::workload(i,task_iterations(i,iterations,distribution)),std::memory_order_relaxed);seen[i].fetch_add(1,std::memory_order_relaxed);},distribution=="skewed"?std::optional<std::size_t>(0):(distribution=="heterogeneous"?std::optional<std::size_t>(i%threads):std::nullopt)));
  const auto submitted=std::chrono::steady_clock::now();
  if(phase=="prequeued"){started=submitted;pool.resume();}
  for(auto& f:futures)f.get();
  pool.wait();
  auto ended=std::chrono::steady_clock::now();
  std::uint64_t checksum=0;for(std::size_t i=0;i<tasks;++i){if(seen[i].load()!=1)throw std::runtime_error("task identity executed other than once");checksum+=values[i].load();}
  double seconds=std::chrono::duration<double>(ended-started).count();
  auto submit_seconds=std::chrono::duration<double>(submitted-enqueue_started).count();
  std::cout<<"mode,threads,tasks,iterations,distribution,phase,seconds,submit_seconds,tasks_per_second,checksum,exactly_once\n"<<mode<<','<<threads<<','<<tasks<<','<<iterations<<','<<distribution<<','<<phase<<','<<seconds<<','<<submit_seconds<<','<<tasks/seconds<<','<<checksum<<",true\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
