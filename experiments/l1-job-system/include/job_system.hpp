#pragma once
#include "work_queue.hpp"
#include <atomic>
#include <condition_variable>
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
class JobSystem {
    struct Wake { std::mutex mutex; std::condition_variable cv; std::size_t generation{}; };
public:
    explicit JobSystem(std::size_t count, bool stealing=false, bool paused=false)
      : stealing_(stealing), started_(!paused) {
        if(!count || count>256) throw std::invalid_argument("threads must be 1..256");
        for(std::size_t i=0;i<count;++i) {
            queues_.push_back(std::make_unique<WorkQueue>());
            wakes_.push_back(std::make_unique<Wake>());
        }
        try {for(std::size_t i=0;i<count;++i)workers_.emplace_back([this,i]{run(i);});if(paused)pause();}
        catch(...) {shutdown();throw;}
    }
    ~JobSystem(){shutdown();}
    JobSystem(const JobSystem&)=delete;
    JobSystem& operator=(const JobSystem&)=delete;
    std::future<void> submit(Job job,std::optional<std::size_t> target=std::nullopt) {
        // Lifecycle gate excludes shutdown; completion never acquires it.
        std::shared_lock gate(lifecycle_);
        if(stopping_.load())throw std::logic_error("pool is closed");
        auto owner=target?*target:next_.fetch_add(1)%queues_.size();
        if(owner>=queues_.size())throw std::out_of_range("target queue");
        auto task=std::make_shared<std::packaged_task<void()>>(std::move(job));
        auto result=task->get_future();
        pending_.fetch_add(1);
        try {queues_[owner]->push([task]{(*task)();});}
        catch(...) {complete();throw;}
        signal(owner);
        if(stealing_) {
            auto thief=wake_cursor_.fetch_add(1)%queues_.size();
            if(thief!=owner)signal(thief);
        }
        return result;
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
        {std::lock_guard lock(wake.mutex);++wake.generation;}
        wake.cv.notify_one();
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
            wake.cv.wait(lock,[this,&wake,observed]{return !started_.load() || wake.generation!=observed || (stopping_.load()&&pending_.load()==0);});
        }
    }
    bool stealing_;
    std::atomic<bool> started_;
    std::size_t parked_{};
    std::shared_mutex lifecycle_;
    std::mutex start_mutex_,idle_mutex_,shutdown_mutex_;
    std::condition_variable start_cv_,idle_;
    std::atomic<std::size_t> pending_{0},next_{0},wake_cursor_{0};
    std::atomic<bool> stopping_{false};
    std::vector<std::unique_ptr<WorkQueue>> queues_;
    std::vector<std::unique_ptr<Wake>> wakes_;
    std::vector<std::thread> workers_;
};
}
