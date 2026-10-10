#include "job_system.hpp"
#include "workload.hpp"
#include "workload_shape.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <numeric>

int main(int argc,char** argv){
 try{
  if(argc>=4 && argc<=8 && std::string(argv[1])=="reference"){
   auto tasks=std::stoull(argv[2]),iterations=std::stoull(argv[3]);std::uint64_t expected=0;
   std::string distribution=argc>=5?argv[4]:"balanced";
   auto seed=argc>=6?std::stoull(argv[5]):0ULL;
   const auto shape=engine_labs::workload_shape(tasks,iterations,distribution,seed,argc>=7?std::stoull(argv[6]):0,argc>=8?std::stoull(argv[7]):100);
   for(std::size_t i=0;i<tasks;++i)expected+=engine_labs::workload(i,shape.iterations[i]);
   std::cout<<expected<<'\n';return 0;
  }
  if(argc<6 || argc>10)throw std::invalid_argument("usage: job_benchmark MODE THREADS TASKS ITERATIONS DISTRIBUTION [PHASE [SEED [HEAVY_PERMILLE [MAX_MULTIPLIER]]]]");
  std::string mode=argv[1],distribution=argv[5],phase=argc>=7?argv[6]:"end_to_end";
  if(mode!="baseline"&&mode!="stealing")throw std::invalid_argument("mode");
  if(phase!="prequeued"&&phase!="end_to_end")throw std::invalid_argument("phase");
  auto threads=std::stoull(argv[2]),tasks=std::stoull(argv[3]),iterations=std::stoull(argv[4]);
  if(!threads||threads>256)throw std::invalid_argument("threads");
  auto seed=argc>=8?std::stoull(argv[7]):0ULL;
  const auto shape=engine_labs::workload_shape(tasks,iterations,distribution,seed,argc>=9?std::stoull(argv[8]):0,argc>=10?std::stoull(argv[9]):100);
  std::vector<std::size_t> queue_heavy(threads),queue_cost(threads);
  for(std::size_t i=0;i<tasks;++i){auto owner=distribution=="skewed"?0:i%threads;queue_cost[owner]+=shape.iterations[i];if(shape.iterations[i]!=iterations)++queue_heavy[owner];}
  std::vector<std::atomic<unsigned>> seen(tasks);
  std::vector<std::atomic<std::uint64_t>> values(tasks);
  // Full-size untimed warmup on the same pool, including all worker queues.
  engine_labs::JobSystem pool(threads,mode=="stealing");
  {
   auto& warm=pool;
   for(std::size_t i=0;i<tasks;++i)warm.submit([i,&shape]{static thread_local volatile std::uint64_t sink; sink=engine_labs::workload(i,shape.iterations[i]);(void)sink;},distribution=="skewed"?std::optional<std::size_t>(0):(engine_labs::cost_skew(distribution)?std::optional<std::size_t>(i%threads):std::nullopt));
   warm.wait();
  }
  if(phase=="prequeued")pool.pause();
  std::vector<std::future<void>> futures;futures.reserve(tasks);
  auto started=std::chrono::steady_clock::now();
  const auto enqueue_started=started;
  for(std::size_t i=0;i<tasks;++i)futures.push_back(pool.submit([&,i]{values[i].store(engine_labs::workload(i,shape.iterations[i]),std::memory_order_relaxed);seen[i].fetch_add(1,std::memory_order_relaxed);},distribution=="skewed"?std::optional<std::size_t>(0):(engine_labs::cost_skew(distribution)?std::optional<std::size_t>(i%threads):std::nullopt)));
  const auto submitted=std::chrono::steady_clock::now();
  if(phase=="prequeued"){started=submitted;pool.resume();}
  for(auto& f:futures)f.get();
  pool.wait();
  auto ended=std::chrono::steady_clock::now();
  std::uint64_t checksum=0;for(std::size_t i=0;i<tasks;++i){if(seen[i].load()!=1)throw std::runtime_error("task identity executed other than once");checksum+=values[i].load();}
  double seconds=std::chrono::duration<double>(ended-started).count();
  auto submit_seconds=std::chrono::duration<double>(submitted-enqueue_started).count();
  std::cout<<"mode,threads,tasks,iterations,distribution,phase,seconds,submit_seconds,tasks_per_second,checksum,exactly_once,seed,heavy_tasks,shape_hash,queue_heavy_counts,queue_iteration_totals\n"<<mode<<','<<threads<<','<<tasks<<','<<iterations<<','<<distribution<<','<<phase<<','<<seconds<<','<<submit_seconds<<','<<tasks/seconds<<','<<checksum<<",true,"<<seed<<','<<shape.heavy<<','<<shape.hash<<',';
  for(std::size_t i=0;i<threads;++i){std::cout<<(i?"|":"")<<queue_heavy[i];}std::cout<<',';
  for(std::size_t i=0;i<threads;++i){std::cout<<(i?"|":"")<<queue_cost[i];}std::cout<<'\n';
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
