#include "../scheduler_adapter.hpp"
#include "../../l1-job-system/include/job_system.hpp"
namespace ecl::adapter {
void schedule_jobs(std::size_t n, std::size_t threads, bool stealing, const Task& task) {
    engine_labs::JobSystem jobs(threads, stealing);
    std::vector<std::future<void>> pending;
    for (std::size_t i=0;i<n;++i) pending.push_back(jobs.submit([&,i]{task(i);}));
    for (auto& future:pending) future.get();
    jobs.wait();
}
}
