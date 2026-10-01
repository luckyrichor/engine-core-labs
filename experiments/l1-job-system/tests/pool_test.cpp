#include "job_system.hpp"
#include <atomic>
#include <iostream>
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
    if (!engine_labs::WorkQueue::stealing_implemented) {
        try { engine_labs::JobSystem unavailable(2, true); }
        catch (const std::logic_error&) { return 0; }
        throw std::runtime_error("fake stealing accepted");
    }
}
