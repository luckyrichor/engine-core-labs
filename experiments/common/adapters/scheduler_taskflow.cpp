#include "../scheduler_adapter.hpp"
#include <taskflow/taskflow.hpp>
namespace ecl::adapter {
void schedule_taskflow(std::size_t n, std::size_t threads, const Task& task) {
    tf::Executor executor(threads);
    tf::Taskflow graph;
    for (std::size_t i=0;i<n;++i) graph.emplace([&,i]{task(i);});
    executor.run(graph).get();
}
}
