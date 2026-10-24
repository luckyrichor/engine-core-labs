#include "../scheduler_adapter.hpp"
#include <future>
namespace ecl::adapter {
void schedule_async(std::size_t n, std::size_t threads, const Task& task) {
    std::vector<std::future<void>> pending;
    for (std::size_t t=0;t<threads;++t) pending.push_back(std::async(std::launch::async,[&,t]{
        for (std::size_t i=t;i<n;i+=threads) task(i);
    }));
    for (auto& future:pending) future.get();
}
}
