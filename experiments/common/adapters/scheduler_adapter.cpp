#include "../scheduler_adapter.hpp"
#include <atomic>
#include <stdexcept>

namespace ecl::adapter {
ScheduleResult execute(const std::string& impl, std::span<const std::uint32_t> costs, std::size_t threads) {
    if (!threads || threads>256) throw std::invalid_argument("threads must be 1..256");
    std::vector<std::atomic<std::uint32_t>> counts(costs.size());
    std::vector<std::atomic<std::uint64_t>> sums(costs.size()), digests(costs.size());
    const Task task = [&](std::size_t i) {
        if (i>=costs.size()) throw std::out_of_range("task index");
        std::uint64_t digest=i+1;
        for (std::uint32_t k=0; k<costs[i]; ++k) digest=(digest^(digest>>7))*0x9e3779b97f4a7c15ULL+k;
        digests[i].store(digest, std::memory_order_relaxed);
        sums[i].fetch_add(costs[i], std::memory_order_relaxed);
        counts[i].fetch_add(1, std::memory_order_relaxed);
    };
    if (impl=="jobs-steal" || impl=="jobs-no-steal") schedule_jobs(costs.size(),threads,impl=="jobs-steal",task);
    else if (impl=="std-async") schedule_async(costs.size(),threads,task);
#ifdef ECL_HAVE_TBB
    else if (impl=="oneTBB") schedule_tbb(costs.size(),threads,task);
#endif
#ifdef ECL_HAVE_TASKFLOW
    else if (impl=="Taskflow") schedule_taskflow(costs.size(),threads,task);
#endif
    else throw std::invalid_argument("scheduler unavailable or unknown: "+impl);
    ScheduleResult result;
    for (std::size_t i=0;i<costs.size();++i) {
        result.checksum+=sums[i].load();
        result.executions.push_back(counts[i].load());
        result.work_digests.push_back(digests[i].load());
    }
    return result;
}
}
