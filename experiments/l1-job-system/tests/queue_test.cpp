#include "work_queue.hpp"
#include <stdexcept>
#include <thread>
#include <vector>
int main() {
    engine_labs::WorkQueue q;
    if (q.pop()) throw std::runtime_error("empty queue");
    int result = 0;
    q.push([&] { result = 1; });
    q.push([&] { result = 2; });
    (*q.pop())();
    if (result != 2) throw std::runtime_error("owner must pop LIFO");
    (*q.pop())();
    if (result != 1 || q.pop()) throw std::runtime_error("drain");
    std::vector<std::thread> producers;
    for (int i=0; i<4; ++i) producers.emplace_back([&] {
        for (int j=0; j<100; ++j) q.push([] {});
    });
    for (auto& t : producers) t.join();
    int count = 0;
    while (q.pop()) ++count;
    if (count != 400) throw std::runtime_error("lost push");
}
