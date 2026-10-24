#include "scheduler.hpp"
#include "work.hpp"
#include "parallel_for.hpp"
#include "../include/job_system.hpp"
#include "../../common/workload_io.hpp"
#include <algorithm>
#include <atomic>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#ifdef ECL_HAVE_TBB
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
#endif
#ifdef ECL_HAVE_TASKFLOW
#include <taskflow/taskflow.hpp>
#endif
using namespace engine_labs::phase3;
using engine_labs::JobSystem;
namespace w=ecl::workload;
struct Args {
    std::map<std::string,std::string> values;
    Args(int argc,char** argv){for(int i=1;i<argc;i+=2){if(i+1==argc)throw std::invalid_argument("option requires value");if(!values.emplace(argv[i],argv[i+1]).second)throw std::invalid_argument("duplicate option");}}
    std::string get(std::string key,std::string fallback="")const{auto it=values.find(key);return it==values.end()?fallback:it->second;}
    std::size_t number(std::string key,std::size_t fallback)const{return std::stoull(get(key,std::to_string(fallback)));}
};
void metric(const std::string& name,double value,const std::string& unit,const std::string& better="lower") {
    if(!std::isfinite(value))throw std::runtime_error("nonfinite metric");
    std::cout<<name<<','<<std::setprecision(17)<<value<<','<<unit<<','<<better<<",1,,0\n";
}
double quantile(std::vector<double> values,double q){std::sort(values.begin(),values.end());return values.at(static_cast<std::size_t>(std::ceil(q*values.size()))-1);}
int main(int argc,char** argv){
 try {
    Args args(argc,argv);
    if(args.get("--action")=="calibrate") {
        std::vector<double> values;std::uint64_t sink=0;
        for(int round=0;round<7;++round){auto t=Clock::now();sink^=work(static_cast<std::uint64_t>(round),4000000);values.push_back(ns(Clock::now()-t)/4000000);}
        std::sort(values.begin(),values.end());std::cout<<std::setprecision(17)<<values[3]<<'\n';std::cerr<<"calibration digest="<<sink<<'\n';return 0;
    }
    const auto meta_path=args.get("--meta");const auto meta=w::verify_meta(meta_path);
    w::require(meta.at("kind").str()=="task_costs","requires task_costs");
    if(!args.get("--fingerprint").empty())w::require(meta.at("workload_fingerprint").str()==args.get("--fingerprint"),"fingerprint mismatch");
    const auto costs=w::read_task_costs(w::verified_file(meta_path,meta,meta.at("files").object.begin()->first));
    w::require(costs.size()==meta.at("answer").at("task_count").u64(),"task count mismatch");
    w::require(std::accumulate(costs.begin(),costs.end(),std::uint64_t{0})==meta.at("answer").at("cost_sum").u64(),"cost sum mismatch");
    const auto threads=args.number("--threads",4), block=args.number("--block",1), producers=args.number("--producers",1);
    const auto impl=args.get("--impl","static"), mode=args.get("--mode","end_to_end");
    const bool profile=args.number("--profile",0)!=0;
    w::require(threads>0 && threads<=256 && producers>0 && producers<=4 && block>0,"invalid counts");
    w::require(mode=="prequeued" || mode=="end_to_end","invalid mode");
    const auto jobs=(costs.size()+block-1)/block;
    std::vector<std::atomic<std::uint32_t>> seen(costs.size());
    std::vector<std::atomic<std::uint64_t>> digests(costs.size());
    const auto body=[&](std::size_t id){digests[id].store(work(id,costs[id]),std::memory_order_relaxed);seen[id].fetch_add(1,std::memory_order_relaxed);};
    const auto task=[&](std::size_t job){const auto end=std::min(costs.size(),job*block+block);for(std::size_t i=job*block;i<end;++i)body(i);};
    std::vector<double> batch_times;
    std::vector<WorkerStats> stats;
    std::vector<engine_labs::SubmissionTiming> profiles(producers);
    Clock::time_point begin,end;
    double submit_ns=0;
    if(impl=="serial") {
        begin=Clock::now();for(std::size_t i=0;i<jobs;++i)task(i);end=Clock::now();
    } else if(impl=="static" || impl=="one" || impl=="half" || impl=="chase") {
        Scheduler pool(threads,jobs,impl,task);
        if(mode=="end_to_end"){pool.resume();begin=Clock::now();}
        const auto submission=Clock::now();
        for(std::size_t i=0;i<jobs;++i)pool.submit(i,i%threads);
        submit_ns=ns(Clock::now()-submission);
        if(mode=="prequeued"){begin=Clock::now();pool.resume();}
        stats=pool.join();end=Clock::now(); // includes final worker join; report this fixed cost
    } else if(impl=="legacy" || impl=="sleepers") {
        // sleepers is enabled only after S2 profiling selects this one change.
        JobSystem pool(threads,true,mode=="prequeued",impl=="sleepers");
        std::latch launch(1);
        std::vector<std::vector<std::future<void>>> futures(producers);
        std::vector<std::vector<double>> batches(producers);
        std::vector<std::thread> submitters;
        for(std::size_t p=0;p<producers;++p)submitters.emplace_back([&,p]{
            launch.wait();
            for(std::size_t first=p;first<jobs;first+=producers*32){
                const auto t=Clock::now();
                for(std::size_t j=0;j<32;++j){auto id=first+j*producers;if(id>=jobs)break;
                    futures[p].push_back(pool.submit([&,id]{task(id);},id%threads,profile?&profiles[p]:nullptr));}
                batches[p].push_back(ns(Clock::now()-t));
            }
        });
        begin=Clock::now();const auto submission=begin;launch.count_down();
        for(auto& t:submitters)t.join();submit_ns=ns(Clock::now()-submission);
        if(mode=="prequeued"){begin=Clock::now();pool.resume();}
        pool.wait();end=Clock::now();
        for(auto& fs:futures)for(auto& f:fs)f.get();
        for(const auto& values:batches)batch_times.insert(batch_times.end(),values.begin(),values.end());
    } else if(impl=="async") {
        std::latch gate(1);std::vector<std::future<void>> fs;
        if(mode=="end_to_end")begin=Clock::now();
        for(std::size_t t=0;t<threads;++t)fs.push_back(std::async(std::launch::async,[&,t]{gate.wait();for(std::size_t i=t;i<jobs;i+=threads)task(i);}));
        if(mode=="prequeued")begin=Clock::now();
        gate.count_down();for(auto& f:fs)f.get();end=Clock::now();
#ifdef ECL_HAVE_TBB
    } else if(impl=="tbb") {
        oneapi::tbb::task_arena arena(static_cast<int>(threads));
        arena.execute([&]{
            oneapi::tbb::task_group group;std::latch gate(1);
            if(mode=="end_to_end"){begin=Clock::now();gate.count_down();}
            for(std::size_t i=0;i<jobs;++i)group.run([&,i]{gate.wait();task(i);});
            if(mode=="prequeued"){begin=Clock::now();gate.count_down();}
            group.wait();end=Clock::now();
        });
#endif
#ifdef ECL_HAVE_TASKFLOW
    } else if(impl=="taskflow") {
        tf::Executor executor(threads);tf::Taskflow graph;std::latch gate(1);
        if(mode=="end_to_end"){begin=Clock::now();gate.count_down();}
        // Use the core-independent blocking interface; one graph task per block.
        parallel_for(costs.size(),block,[&](auto fn){graph.emplace([&,fn]{gate.wait();fn();});},body);
        auto future=executor.run(graph);
        if(mode=="prequeued"){begin=Clock::now();gate.count_down();}
        future.get();end=Clock::now();
#endif
    } else throw std::invalid_argument("unknown/unavailable implementation");
    for(std::size_t i=0;i<costs.size();++i){w::require(seen[i].load()==1,"task not exactly once");w::require(digests[i].load()==work(i,costs[i]),"work digest mismatch");}
    const auto elapsed=ns(end-begin);w::require(elapsed>0,"invalid duration");
    std::cout<<"metric,value,unit,better,valid,invalid_reason,stub\n";
    metric("throughput",double(costs.size())*1e9/elapsed,"tasks/s","higher");
    metric("elapsed_ns",elapsed,"ns");metric("submitted_tasks",double(jobs),"count");
    metric("submit_ns",submit_ns,"ns");
    std::vector<std::uint64_t> loads(threads);
    for(std::size_t i=0;i<jobs;++i)for(std::size_t k=i*block;k<std::min(costs.size(),i*block+block);++k)loads[i%threads]+=costs[k];
    const auto sum=std::accumulate(loads.begin(),loads.end(),std::uint64_t{0});
    metric("static_imbalance",sum?double(*std::max_element(loads.begin(),loads.end()))*double(threads)/double(sum):1,"ratio");
    if(!stats.empty()) {
        std::uint64_t attempts=0,successes=0,stolen=0;double idle=0,search=0;
        for(auto s:stats){attempts+=s.attempts;successes+=s.successes;stolen+=s.stolen;idle+=s.idle_ns;search+=s.search_ns;}
        metric("steal_attempts",double(attempts),"count");metric("stolen_tasks",double(stolen),"count");
        metric("steal_success_rate",attempts?double(successes)/double(attempts):0,"ratio","higher");
        metric("idle_fraction",idle/(elapsed*double(threads)),"ratio");metric("search_fraction",search/(elapsed*double(threads)),"ratio");
    }
    if(!batch_times.empty()){
        metric("submit_batch_p50",quantile(batch_times,.5),"ns");metric("submit_batch_p99",quantile(batch_times,.99),"ns");metric("submit_batch_samples",double(batch_times.size()),"count","higher");
    }
    if(profile){engine_labs::SubmissionTiming total;for(auto p:profiles){total.gate_ns+=p.gate_ns;total.prepare_ns+=p.prepare_ns;total.enqueue_ns+=p.enqueue_ns;total.wake_ns+=p.wake_ns;}
        metric("profile_gate_ns",total.gate_ns,"ns");metric("profile_prepare_ns",total.prepare_ns,"ns");metric("profile_enqueue_ns",total.enqueue_ns,"ns");metric("profile_wake_ns",total.wake_ns,"ns");}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
