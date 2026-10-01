#include "job_system.hpp"
#include "workload.hpp"
#include <chrono>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    try {
        if(argc!=6)throw std::invalid_argument("usage: job_benchmark baseline|stealing THREADS TASKS ITERATIONS balanced|skewed");
        const std::string mode=argv[1], distribution=argv[5];
        if(mode!="baseline" && mode!="stealing")throw std::invalid_argument("unknown mode");
        if(distribution!="balanced" && distribution!="skewed")throw std::invalid_argument("unknown distribution");
        if (mode=="stealing" && !engine_labs::WorkQueue::stealing_implemented) {
            std::cerr<<"BLOCKED: user must implement steal(); no stealing measurements produced\n";
            return 2;
        }
        const auto threads=std::stoull(argv[2]), tasks=std::stoull(argv[3]), iterations=std::stoull(argv[4]);
        if(!tasks || tasks>10000000 || !iterations || iterations>100000000)
            throw std::invalid_argument("invalid workload bounds");
        std::atomic<std::uint64_t> checksum=0;
        engine_labs::JobSystem pool(threads,mode=="stealing");
        const auto started=std::chrono::steady_clock::now();
        std::vector<std::future<void>> futures;futures.reserve(tasks);
        for(std::size_t i=0;i<tasks;++i)futures.push_back(pool.submit([&,i]{
            checksum.fetch_xor(engine_labs::workload(i,iterations),std::memory_order_relaxed);
        },distribution=="skewed"?std::optional<std::size_t>(0):std::nullopt));
        for(auto& f:futures) { f.get(); }
        pool.wait();
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        std::cout<<"mode,threads,tasks,iterations,distribution,seconds,tasks_per_second,checksum\n"
                 <<mode<<','<<threads<<','<<tasks<<','<<iterations<<','<<distribution<<','
                 <<seconds<<','<<tasks/seconds<<','<<checksum.load()<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
