#include "job_system.hpp"
#include <atomic>
#include <iostream>
void executor_contracts();
int main() {
    std::atomic<int> count{0};
    {
        engine_labs::JobSystem pool(4);
        std::vector<std::future<void>> futures;
        for (int i=0; i<2000; ++i) futures.push_back(pool.submit([&] { ++count; }));
        auto failure = pool.submit([] { throw std::runtime_error("task failed"); });
        for (auto& future : futures) future.get();
        bool propagated = false;
        try { failure.get(); } catch (const std::runtime_error&) { propagated = true; }
        if (!propagated) throw std::runtime_error("lost task failure");
        pool.wait();
        for (int i=0; i<100; ++i) pool.submit([&] { ++count; }, 0);
        // Destructor must drain even the deliberately imbalanced queue.
    }
    if (count != 2100) throw std::runtime_error("lost or duplicated tasks");
    bool refused = false;
    try { engine_labs::JobSystem invalid(0); } catch (const std::invalid_argument&) { refused = true; }
    if (!refused) throw std::runtime_error("accepted zero threads");
    executor_contracts();
}

// Stress both policies with identity checks, multi-producer submission, repeated
// pause/resume barriers and closed submission. No timing-dependent success test.
void executor_contracts(){
  for(bool steal:{false,true}) {
   engine_labs::JobSystem pool(4,steal,true);
   std::vector<std::atomic<int>> seen(4000);
   std::vector<std::thread> producers;
   for(int p=0;p<4;++p)producers.emplace_back([&,p]{for(int i=0;i<1000;++i){int id=p*1000+i;pool.submit([&,id]{++seen[id];},0);}});
   for(auto& p:producers)p.join();
   for(auto& item:seen)if(item.load()!=0)throw std::runtime_error("paused worker executed");
   pool.resume();pool.wait();
   for(auto& item:seen)if(item.load()!=1)throw std::runtime_error("executor identity count");
   for(int round=0;round<20;++round){pool.pause();auto task=pool.submit([]{},0);pool.resume();task.get();pool.wait();}
   pool.shutdown();bool closed=false;try{pool.submit([]{});}catch(const std::logic_error&){closed=true;}
   if(!closed)throw std::runtime_error("closed submit accepted");
  }
}
