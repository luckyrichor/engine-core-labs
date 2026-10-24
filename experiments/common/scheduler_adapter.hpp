#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ecl::adapter {
struct ScheduleResult {
    std::uint64_t checksum{};
    std::vector<std::uint32_t> executions;
    std::vector<std::uint64_t> work_digests;
};
using Task = std::function<void(std::size_t)>;
void schedule_jobs(std::size_t n, std::size_t threads, bool stealing, const Task& task);
void schedule_async(std::size_t n, std::size_t threads, const Task& task);
#ifdef ECL_HAVE_TBB
void schedule_tbb(std::size_t n, std::size_t threads, const Task& task);
#endif
#ifdef ECL_HAVE_TASKFLOW
void schedule_taskflow(std::size_t n, std::size_t threads, const Task& task);
#endif
// Costs are integer work units from workload v1, not a calibrated duration.
ScheduleResult execute(const std::string& impl, std::span<const std::uint32_t> costs, std::size_t threads);
}
