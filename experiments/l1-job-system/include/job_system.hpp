#pragma once
#include "work_queue.hpp"
#include "../../common/cache_cell.hpp"
#include <array>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <future>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace engine_labs {
// Mutex deques plus per-worker sleep notifications. Both policies use the same
// executor; only cross-queue stealing differs. shutdown/wait must be called by
// an external thread, never by a task waiting for its own completion.
struct SubmissionTiming { double gate_ns{},prepare_ns{},enqueue_ns{},wake_ns{}; };
template<int PaddedCounter=0>
class BasicJobSystem {
    static_assert(PaddedCounter>=0 && PaddedCounter<=3);
    struct Wake { std::mutex mutex; std::condition_variable cv; std::size_t generation{}; bool sleeping{}; };
public:
    explicit BasicJobSystem(std::size_t count, bool stealing=false, bool paused=false, bool wake_sleepers_only=false)
      : stealing_(stealing), wake_sleepers_only_(wake_sleepers_only), started_(!paused) {
        if(!count || count>256) throw std::invalid_argument("threads must be 1..256");
        for(std::size_t i=0;i<count;++i) {
            queues_.push_back(std::make_unique<WorkQueue>());
            wakes_.push_back(std::make_unique<Wake>());
        }
        try {for(std::size_t i=0;i<count;++i)workers_.emplace_back([this,i]{run(i);});if(paused)pause();}
        catch(...) {shutdown();throw;}
    }
    ~BasicJobSystem(){shutdown();}
    BasicJobSystem(const BasicJobSystem&)=delete;
    BasicJobSystem& operator=(const BasicJobSystem&)=delete;
    std::future<void> submit(Job job,std::optional<std::size_t> target=std::nullopt, SubmissionTiming* timing=nullptr) {
        using ProfileClock=std::chrono::steady_clock;
        auto previous=timing ? ProfileClock::now() : ProfileClock::time_point{};
        auto sample=[&](double& field){auto now=ProfileClock::now();field+=std::chrono::duration<double,std::nano>(now-previous).count();previous=now;};
        // Lifecycle gate excludes shutdown; completion never acquires it.
        std::shared_lock gate(lifecycle_);
        if(timing)sample(timing->gate_ns);
        if(stopping_.load())throw std::logic_error("pool is closed");
        auto owner=target?*target:next_.fetch_add(1)%queues_.size();
        if(owner>=queues_.size())throw std::out_of_range("target queue");
        auto task=std::make_shared<std::packaged_task<void()>>(std::move(job));
        auto result=task->get_future();
        pending_.fetch_add(1);
        if(timing)sample(timing->prepare_ns);
        try {queues_[owner]->push([task]{(*task)();});}
        catch(...) {complete();throw;}
        if(timing)sample(timing->enqueue_ns);
        signal(owner);
        if(stealing_) {
            auto thief=wake_cursor_.fetch_add(1)%queues_.size();
            if(thief!=owner)signal(thief);
        }
        if(timing)sample(timing->wake_ns);
        return result;
    }
    void configure_workers(const std::function<void(std::thread::native_handle_type,std::size_t)>& configure) {
        if(started_.load() || stopping_.load())throw std::logic_error("worker setup requires paused pool");
        for(std::size_t i=0;i<workers_.size();++i)configure(workers_[i].native_handle(),i);
    }
    std::array<std::size_t,4> counter_layout() const {
        const auto base=reinterpret_cast<std::uintptr_t>(this);
        return {sizeof(*this),reinterpret_cast<std::uintptr_t>(&pending_)-base,
                reinterpret_cast<std::uintptr_t>(&next_)-base,reinterpret_cast<std::uintptr_t>(&wake_cursor_)-base};
    }
    void pause(){
        std::unique_lock lock(start_mutex_);started_.store(false);
        for(std::size_t i=0;i<wakes_.size();++i)signal(i);
        start_cv_.wait(lock,[this]{return parked_==workers_.size();});
    }
    void resume(){
        {std::lock_guard lock(start_mutex_);started_.store(true);}
        start_cv_.notify_all(); // One-time start barrier, not a per-task broadcast.
    }
    void wait(){
        std::unique_lock lock(idle_mutex_);
        idle_.wait(lock,[this]{return pending_.load()==0;});
    }
    void shutdown(){
        std::lock_guard join(shutdown_mutex_);
        {std::unique_lock gate(lifecycle_);stopping_.store(true);}
        resume();
        for(std::size_t i=0;i<wakes_.size();++i)signal(i);
        for(auto& worker:workers_)if(worker.joinable())worker.join();
    }
private:
    void signal(std::size_t worker){
        auto& wake=*wakes_[worker];
        bool notify;
        {std::lock_guard lock(wake.mutex);++wake.generation;notify=!wake_sleepers_only_ || wake.sleeping;}
        // Always publish a generation, even when not notifying: a worker between
        // scan and wait must see the pending signal in the predicate. sleeping
        // is set under this same mutex before wait atomically releases it.
        if(notify)wake.cv.notify_one();
    }
    void complete(){
        if(pending_.fetch_sub(1)==1){
            // Pair with wait's mutex to avoid a lost zero-pending notification.
            {std::lock_guard lock(idle_mutex_);idle_.notify_all();}
            if(stopping_.load())for(std::size_t i=0;i<wakes_.size();++i)signal(i);
        }
    }
    void run(std::size_t owner){
        auto& wake=*wakes_[owner];
        for(;;){
            if(!started_.load()){
                std::unique_lock lock(start_mutex_);
                if(!started_.load()){
                    ++parked_;start_cv_.notify_all();
                    start_cv_.wait(lock,[this]{return started_.load();});
                    --parked_;
                }
            }
            std::size_t observed;
            {std::lock_guard lock(wake.mutex);observed=wake.generation;}
            auto job=queues_[owner]->pop();
            if(!job && stealing_)for(std::size_t offset=1;offset<queues_.size();++offset){
                job=queues_[(owner+offset)%queues_.size()]->steal();if(job)break;
            }
            if(job){(*job)();complete();continue;}
            std::unique_lock lock(wake.mutex);
            if(stopping_.load() && pending_.load()==0)return;
            wake.sleeping=true;
            wake.cv.wait(lock,[this,&wake,observed]{return !started_.load() || wake.generation!=observed || (stopping_.load()&&pending_.load()==0);});
            wake.sleeping=false;
        }
    }
    bool stealing_,wake_sleepers_only_;
    std::atomic<bool> started_;
    std::size_t parked_{};
    std::shared_mutex lifecycle_;
    std::mutex start_mutex_,idle_mutex_,shutdown_mutex_;
    std::condition_variable start_cv_,idle_;
    AtomicCell<std::size_t,PaddedCounter==1> pending_{0};
    AtomicCell<std::size_t,PaddedCounter==2> next_{0};
    AtomicCell<std::size_t,PaddedCounter==3> wake_cursor_{0};
    std::atomic<bool> stopping_{false};
    std::vector<std::unique_ptr<WorkQueue>> queues_;
    std::vector<std::unique_ptr<Wake>> wakes_;
    std::vector<std::thread> workers_;
};
using JobSystem=BasicJobSystem<0>;
}
