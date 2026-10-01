#pragma once
#include <deque>
#include <functional>
#include <mutex>
#include <optional>

namespace engine_labs {
using Job = std::function<void()>;
class WorkQueue {
public:
    void push(Job job) {
        std::lock_guard lock(mutex_);
        jobs_.push_front(std::move(job));
    }
    std::optional<Job> pop() {
        std::lock_guard lock(mutex_);
        if (jobs_.empty()) return std::nullopt;
        auto job = std::move(jobs_.front());
        jobs_.pop_front();
        return job;
    }
    // USER TODO: lock victim queue; remove from its back, return nullopt if empty.
    // Keep this mutex-based exercise correct before considering a lock-free deque.
    std::optional<Job> steal() {
        return std::nullopt; // USER IMPLEMENTATION REQUIRED
    }
    static constexpr bool stealing_implemented = false; // user flips after implementation
private:
    std::mutex mutex_;
    std::deque<Job> jobs_;
};
}
