#include "../scheduler_adapter.hpp"
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
namespace ecl::adapter {
void schedule_tbb(std::size_t n, std::size_t threads, const Task& task) {
    oneapi::tbb::task_arena arena(static_cast<int>(threads));
    arena.execute([&]{
        oneapi::tbb::task_group group;
        for (std::size_t i=0;i<n;++i) group.run([&,i]{task(i);});
        group.wait();
    });
}
}
