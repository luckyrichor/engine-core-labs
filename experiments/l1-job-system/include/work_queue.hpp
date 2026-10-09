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
    // Codex implementation: owner takes newest, thief takes oldest.
    // Check and removal are atomic under the victim lock; execute after unlock.
    std::optional<Job> steal() {
        std::lock_guard lock(mutex_);
        if (jobs_.empty()) return std::nullopt;
        auto job = std::move(jobs_.back());
        jobs_.pop_back();
        return job;
    }
    static constexpr bool stealing_implemented = true; // completed implementation
private:
    std::mutex mutex_;
    std::deque<Job> jobs_;
};
}
