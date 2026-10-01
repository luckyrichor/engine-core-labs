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
    // Codex reference implementation, retained by user request on 2026-10-02.
    // USER EXERCISE: replace this body with your own implementation and rerun tests.
    std::optional<Job> steal() {
        std::lock_guard lock(mutex_);
        if (jobs_.empty()) return std::nullopt;
        auto job = std::move(jobs_.back());
        jobs_.pop_back();
        return job;
    }
    static constexpr bool stealing_implemented = true; // Codex reference, user exercise pending
private:
    std::mutex mutex_;
    std::deque<Job> jobs_;
};
}
