#include "work_queue.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
int main() {
    if (!engine_labs::WorkQueue::stealing_implemented) {
        std::cerr << "USER TODO: steal() remains unimplemented\n";
        return 77;
    }
    engine_labs::WorkQueue q;
    int selected=0;
    q.push([&] { selected=1; });q.push([&] { selected=2; });
    auto oldest=q.steal();if (!oldest) throw std::runtime_error("failed steal");(*oldest)();
    if (selected!=1) throw std::runtime_error("thief must take oldest");
    auto owner=q.pop();if (!owner) throw std::runtime_error("lost owner job");(*owner)();
    if (selected!=2 || q.steal()) throw std::runtime_error("empty/owner contract");
    std::atomic<int> count=0;
    for(int i=0;i<10000;++i) q.push([&] { ++count; });
    std::vector<std::thread> consumers;
    for(int i=0;i<8;++i) consumers.emplace_back([&, i] {
        for (;;) { auto job = i==0 ? q.pop() : q.steal(); if (!job) break; (*job)(); }
    });
    for(auto& t:consumers)t.join();
    if(count!=10000)throw std::runtime_error("lost/duplicated concurrent jobs");
    // Aggregate count alone misses one duplicate cancelling one lost job.
    std::vector<std::atomic<int>> seen(10000);
    std::atomic<bool> producer_done=false;
    consumers.clear();
    for(int i=0;i<8;++i) consumers.emplace_back([&, i] {
        for (;;) {
            auto job = i==0 ? q.pop() : q.steal();
            if(job) { (*job)(); continue; }
            if(producer_done.load()) break;
            std::this_thread::yield();
        }
    });
    for(int i=0;i<10000;++i) q.push([&, i] { ++seen[i]; });
    producer_done.store(true);
    for(auto& t:consumers)t.join();
    for(auto& entry:seen)if(entry!=1)throw std::runtime_error("job identity lost or duplicated");
}
