#pragma once
#include "work_queue.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace engine_labs {
class JobSystem {
public:
    explicit JobSystem(std::size_t count, bool stealing = false) : stealing_(stealing) {
        if (!count || count > 256) throw std::invalid_argument("threads must be 1..256");
        if (stealing && !WorkQueue::stealing_implemented)
            throw std::logic_error("USER TODO: implement WorkQueue::steal before stealing experiments");
        for (std::size_t i=0; i<count; ++i) queues_.push_back(std::make_unique<WorkQueue>());
        try {
            for (std::size_t i=0; i<count; ++i) workers_.emplace_back([this, i] { run(i); });
        } catch (...) {
            shutdown();
            throw;
        }
    }
    ~JobSystem() { shutdown(); }
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    std::future<void> submit(Job job, std::optional<std::size_t> target = std::nullopt) {
        auto task = std::make_shared<std::packaged_task<void()>>(std::move(job));
        auto result = task->get_future();
        {
            std::lock_guard lock(state_);
            if (stopping_) throw std::logic_error("pool is closed");
            const auto owner = target.value_or(next_++ % queues_.size());
            if (owner >= queues_.size()) throw std::out_of_range("target queue");
            queues_[owner]->push([task] { (*task)(); });
            ++pending_;
            ++generation_;
        }
        wake_.notify_all();
        return result;
    }
    void wait() {
        std::unique_lock lock(state_);
        idle_.wait(lock, [this] { return pending_ == 0; });
    }
    void shutdown() {
        { std::lock_guard lock(state_); stopping_ = true; ++generation_; }
        wake_.notify_all();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
    }
private:
    void run(std::size_t owner) {
        for (;;) {
            std::size_t observed;
            { std::lock_guard lock(state_); observed = generation_; }
            auto job = queues_[owner]->pop();
            if (!job && stealing_) {
                for (std::size_t offset=1; offset<queues_.size(); ++offset) {
                    job = queues_[(owner+offset)%queues_.size()]->steal();
                    if (job) break;
                }
            }
            if (job) {
                (*job)(); // packaged_task captures exceptions in its future
                {
                    std::lock_guard lock(state_);
                    --pending_;
                    ++generation_;
                }
                idle_.notify_all();
                wake_.notify_all();
                continue;
            }
            std::unique_lock lock(state_);
            if (stopping_ && pending_ == 0) return;
            wake_.wait_for(lock, std::chrono::milliseconds(1), [this, observed] {
                return generation_ != observed || (stopping_ && pending_ == 0);
            });
        }
    }
    bool stealing_;
    std::mutex state_;
    std::condition_variable wake_, idle_;
    std::size_t pending_ = 0, generation_ = 0, next_ = 0;
    bool stopping_ = false;
    std::vector<std::unique_ptr<WorkQueue>> queues_;
    std::vector<std::thread> workers_;
};
}
