#pragma once
#include "chase_lev.hpp"
#include <chrono>
#include <array>
#include <condition_variable>
#include <deque>
#include <functional>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

namespace engine_labs::phase3 {
using Clock=std::chrono::steady_clock;
inline double ns(Clock::duration d) { return std::chrono::duration<double,std::nano>(d).count(); }
struct WorkerStats { std::uint64_t attempts{},successes{},stolen{},tasks{};double idle_ns{},search_ns{}; };
template<bool PaddedStats=false>
class BasicScheduler {
    struct alignas(PaddedStats ? 64 : alignof(WorkerStats)) StatsBlock:WorkerStats {};
    struct Queue {
        std::mutex mutex;
        std::deque<std::size_t> values;
        void push(std::size_t id) { std::lock_guard lock(mutex); values.push_back(id); }
        std::optional<std::size_t> pop() {
            std::lock_guard lock(mutex);
            if(values.empty()) return {};
            auto id=values.back();values.pop_back();return id;
        }
        std::vector<std::size_t> steal(bool half) {
            std::lock_guard lock(mutex);
            std::vector<std::size_t> out;
            const auto count=half ? (values.size()+1)/2 : std::min<std::size_t>(1,values.size());
            out.reserve(count);
            for(std::size_t i=0;i<count;++i) {out.push_back(values.front());values.pop_front();}
            return out;
        }
    };
    struct Worker {
        Queue inbox, local;
        ChaseLev chase;
        std::mutex mutex; std::condition_variable cv;
        std::uint64_t generation{};
        StatsBlock stats;
        explicit Worker(std::size_t capacity):chase(capacity) {}
    };
    std::vector<std::unique_ptr<Worker>> workers_;
    std::vector<std::thread> threads_;
    std::function<void(std::size_t)> task_;
    std::string policy_;
    std::size_t total_;
    std::atomic<std::size_t> done_{0};
    std::atomic<bool> stop_{false};
    std::mutex start_mutex_,error_mutex_;
    std::condition_variable start_cv_;
    bool started_{false};
    std::exception_ptr error_;
    void signal(std::size_t i) {
        auto& w=*workers_[i];
        {std::lock_guard lock(w.mutex);++w.generation;}
        w.cv.notify_one();
    }
    void all() { for(std::size_t i=0;i<workers_.size();++i)signal(i); }
    void run(std::size_t owner,std::latch& ready) {
        auto& w=*workers_[owner];
        ready.count_down();
        {std::unique_lock lock(start_mutex_);start_cv_.wait(lock,[&]{return started_;});}
        while(!stop_.load() && done_.load()<total_) {
            const auto idle_start=Clock::now();
            std::uint64_t observed;
            {std::lock_guard lock(w.mutex);observed=w.generation;}
            // Only this worker transfers its producer inbox into the owner deque.
            bool transferred=false;
            {
                std::lock_guard lock(w.inbox.mutex);
                transferred=!w.inbox.values.empty();
                for(auto id:w.inbox.values) {
                    if(policy_=="chase") {
                        if(!w.chase.push(id)) std::terminate(); // capacity >= whole batch
                    } else w.local.push(id);
                }
                w.inbox.values.clear();
            }
            if(transferred && policy_!="static")all();
            auto id=policy_=="chase" ? w.chase.pop() : w.local.pop();
            if(!id && policy_!="static") {
                for(std::size_t offset=1;offset<workers_.size();++offset) {
                    auto& victim=*workers_[(owner+offset)%workers_.size()];
                    ++w.stats.attempts;
                    if(policy_=="chase") {
                        id=victim.chase.steal();
                        if(id) {++w.stats.successes;++w.stats.stolen;}
                    } else {
                        auto batch=victim.local.steal(policy_=="half");
                        if(!batch.empty()) {
                            ++w.stats.successes;w.stats.stolen+=batch.size();
                            id=batch.front();
                            for(std::size_t j=1;j<batch.size();++j)w.local.push(batch[j]);
                        }
                    }
                    if(id)break;
                }
            }
            if(id) {
                w.stats.search_ns+=ns(Clock::now()-idle_start);
                try {task_(*id);} catch(...) {std::lock_guard lock(error_mutex_);if(!error_)error_=std::current_exception();}
                ++w.stats.tasks;
                if(done_.fetch_add(1)+1==total_)all();
            } else {
                w.stats.search_ns+=ns(Clock::now()-idle_start);
                const auto wait_start=Clock::now();
                std::unique_lock lock(w.mutex);
                w.cv.wait(lock,[&]{return w.generation!=observed || stop_.load() || done_.load()==total_;});
                w.stats.idle_ns+=ns(Clock::now()-wait_start);
            }
        }
    }
public:
    BasicScheduler(std::size_t n,std::size_t total,std::string policy,std::function<void(std::size_t)> task)
      :task_(std::move(task)),policy_(std::move(policy)),total_(total) {
        if(!n || n>256 || (policy_!="static" && policy_!="one" && policy_!="half" && policy_!="chase"))throw std::invalid_argument("scheduler parameters");
        for(std::size_t i=0;i<n;++i)workers_.push_back(std::make_unique<Worker>(total));
        std::latch ready(static_cast<std::ptrdiff_t>(n));
        try {for(std::size_t i=0;i<n;++i)threads_.emplace_back([&,i]{run(i,ready);});ready.wait();}
        catch(...) {stop_.store(true);resume();all();for(auto& t:threads_)if(t.joinable())t.join();throw;}
    }
    ~BasicScheduler() {stop_.store(true);resume();all();for(auto& t:threads_)if(t.joinable())t.join();}
    void configure_workers(const std::function<void(std::thread::native_handle_type,std::size_t)>& configure) {
        std::lock_guard lock(start_mutex_);
        if(started_)throw std::logic_error("worker setup requires parked pool");
        for(std::size_t i=0;i<threads_.size();++i)configure(threads_[i].native_handle(),i);
    }
    std::array<std::size_t,3> stats_layout() const {
        const auto& w=*workers_.front();
        return {sizeof(Worker),sizeof(StatsBlock),reinterpret_cast<std::uintptr_t>(&w.stats)-reinterpret_cast<std::uintptr_t>(&w)};
    }
    void submit(std::size_t id,std::size_t owner) {
        if(owner>=workers_.size() || id>=total_)throw std::out_of_range("task/owner");
        workers_[owner]->inbox.push(id); signal(owner);
        if(policy_!="static" && workers_.size()>1)signal((owner+1)%workers_.size());
    }
    void resume() {{std::lock_guard lock(start_mutex_);started_=true;}start_cv_.notify_all();}
    std::vector<WorkerStats> join() {
        for(auto& t:threads_)if(t.joinable())t.join();
        if(error_)std::rethrow_exception(error_);
        std::vector<WorkerStats> out;for(const auto& w:workers_)out.push_back(w->stats);return out;
    }
};
using Scheduler=BasicScheduler<false>;
}
