#include "platform.hpp"
#include "../include/job_system.hpp"
#include "../phase3/scheduler.hpp"
#include "../phase3/work.hpp"
#include "../../common/workload_io.hpp"
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <latch>
#include <map>
#include <numeric>
#include <sstream>
namespace p=engine_labs::phase4;
namespace w=ecl::workload;
using Clock=std::chrono::steady_clock;
using engine_labs::AtomicCell;
using engine_labs::phase3::work;
struct Args {
    std::map<std::string,std::string> values;
    Args(int argc,char** argv){for(int i=1;i<argc;i+=2){p::require(i+1<argc,"option needs value");p::require(values.emplace(argv[i],argv[i+1]).second,"duplicate option");}}
    std::string get(const std::string& key,const std::string& fallback="")const{auto it=values.find(key);return it==values.end()?fallback:it->second;}
    std::size_t number(const std::string& key,std::size_t fallback)const{return std::stoull(get(key,std::to_string(fallback)));}
};
struct Options {std::size_t threads{},iterations{},units{};bool pinned{};std::vector<int> cpus;};
struct Result {double elapsed{};std::size_t operations{},layout_bytes{};std::vector<int> first,last;};
inline double ns(Clock::duration duration){return std::chrono::duration<double,std::nano>(duration).count();}
template<class F>
Result thread_run(const Options& options,p::PerfControl& perf,F function){
    Result result;result.first.resize(options.threads);result.last.resize(options.threads);
    std::atomic<bool> failed{false};std::exception_ptr error;std::mutex error_mutex;
    std::latch ready(static_cast<std::ptrdiff_t>(options.threads)),gate(1),finished(static_cast<std::ptrdiff_t>(options.threads));
    std::vector<std::thread> workers;
    try {
        for(std::size_t t=0;t<options.threads;++t)workers.emplace_back([&,t]{
            try{if(options.pinned)p::pin_current(options.cpus[t%options.cpus.size()]);result.first[t]=p::current_cpu();}
            catch(...){failed.store(true);std::lock_guard lock(error_mutex);if(!error)error=std::current_exception();}
            ready.count_down();gate.wait();
            if(!failed.load())try{function(t,failed);}catch(...){failed.store(true);std::lock_guard lock(error_mutex);if(!error)error=std::current_exception();}
            result.last[t]=p::current_cpu();finished.count_down();
        });
    }catch(...){failed.store(true);gate.count_down();for(auto& thread:workers)if(thread.joinable())thread.join();throw;}
    ready.wait();
    try {
        if(error)std::rethrow_exception(error);
        perf.enable();const auto begin=Clock::now();gate.count_down();finished.wait();const auto end=Clock::now();perf.disable();
        result.elapsed=ns(end-begin);
    }catch(...){failed.store(true);if(!gate.try_wait())gate.count_down();for(auto& thread:workers)if(thread.joinable())thread.join();throw;}
    for(auto& thread:workers)thread.join();
    if(error)std::rethrow_exception(error);
    if(options.pinned)for(std::size_t t=0;t<options.threads;++t)p::require(result.first[t]==options.cpus[t%options.cpus.size()]&&result.last[t]==result.first[t],"pinned CPU sample mismatch");
    return result;
}
template<bool Padded>
struct alignas(64) CounterArray {AtomicCell<std::uint64_t,Padded> cells[256];};
template<bool Padded>
Result positive(const Options& options,p::PerfControl& perf){
    auto storage=std::make_unique<CounterArray<Padded>>();
    auto* counters=storage->cells;
    auto result=thread_run(options,perf,[&](std::size_t t,std::atomic<bool>&){
        auto* cell=&counters[t];for(std::size_t i=0;i<options.iterations;++i)cell->fetch_add(1,std::memory_order_relaxed);
    });
    for(std::size_t t=0;t<options.threads;++t)p::require(counters[t].load()==options.iterations,"positive counter mismatch");
    result.operations=options.iterations*options.threads;result.layout_bytes=sizeof(counters[0])*options.threads;return result;
}
template<bool Padded>
struct alignas(Padded?64:alignof(std::uint64_t)) Cold {std::uint64_t seed{};};
template<bool Padded>
Result negative(const Options& options,p::PerfControl& perf){
    auto cold=std::make_unique<Cold<Padded>[]>(options.threads);
    std::vector<std::uint64_t> seeds(options.threads),sums(options.threads);
    // The only differently laid out fields are read before the measured window.
    for(std::size_t t=0;t<options.threads;++t){cold[t].seed=t+1;seeds[t]=cold[t].seed;}
    auto result=thread_run(options,perf,[&](std::size_t t,std::atomic<bool>&){
        std::uint64_t sum=0;for(std::size_t i=0;i<options.iterations;++i)sum^=work(i+seeds[t],static_cast<std::uint32_t>(options.units));sums[t]=sum;
    });
    for(std::size_t t=0;t<options.threads;++t){std::uint64_t expected=0;for(std::size_t i=0;i<options.iterations;++i)expected^=work(i+t+1,static_cast<std::uint32_t>(options.units));p::require(sums[t]==expected,"negative checksum mismatch");}
    result.operations=options.iterations*options.threads;result.layout_bytes=sizeof(cold[0])*options.threads;return result;
}
// Verification slots are separate cache lines so that their bookkeeping cannot
// create a second packed writer array in the queue/pool layout comparisons.
struct alignas(64) Checked {std::atomic<std::uint32_t> seen{0};std::atomic<std::uint64_t> digest{0};};
void execute(Checked* values,std::size_t id,const Options& options){
    p::require(id<options.iterations,"task ID out of range");
    values[id].digest.store(work(id,static_cast<std::uint32_t>(options.units)),std::memory_order_relaxed);
    values[id].seen.fetch_add(1,std::memory_order_relaxed);
}
void verify(Checked* values,const Options& options){for(std::size_t i=0;i<options.iterations;++i){p::require(values[i].seen.load()==1,"task ID not exactly once");p::require(values[i].digest.load()==work(i,static_cast<std::uint32_t>(options.units)),"payload mismatch");}}
template<bool Separate>
Result queue_case(const Options& options,p::PerfControl& perf){
    alignas(64) engine_labs::phase3::BasicChaseLev<Separate> queue(options.iterations);
    auto checked=std::make_unique<Checked[]>(options.iterations);std::atomic<std::size_t> done{0};
    const auto consume=[&](std::size_t id){execute(checked.get(),id,options);done.fetch_add(1);};
    auto result=thread_run(options,perf,[&](std::size_t t,std::atomic<bool>& failed){
        if(t==0){
            for(std::size_t id=0;id<options.iterations&&!failed.load();++id){
                while(!queue.push(id)&&!failed.load()){if(auto next=queue.pop())consume(*next);else std::this_thread::yield();}
                if(id%4==0)if(auto next=queue.pop())consume(*next);
            }
            while(done.load()<options.iterations&&!failed.load()){if(auto next=queue.pop())consume(*next);else std::this_thread::yield();}
        }else while(done.load()<options.iterations&&!failed.load()){if(auto next=queue.steal())consume(*next);else std::this_thread::yield();}
    });
    verify(checked.get(),options);result.operations=options.iterations;result.layout_bytes=sizeof(queue);return result;
}
template<int Kind>
Result counter_case(const Options& options,p::PerfControl& perf){
    alignas(64) engine_labs::BasicJobSystem<Kind> pool(options.threads,true,true);
    if(options.pinned)pool.configure_workers([&](auto handle,std::size_t t){p::pin_native(handle,options.cpus[t%options.cpus.size()]);});
    auto checked=std::make_unique<Checked[]>(options.iterations);
    std::vector<std::future<void>> futures;futures.reserve(options.iterations);
    perf.enable();const auto begin=Clock::now();pool.resume();
    for(std::size_t i=0;i<options.iterations;++i)futures.push_back(pool.submit([&,i]{execute(checked.get(),i,options);}));
    pool.wait();const auto end=Clock::now();perf.disable();
    for(auto& future:futures)future.get();
    verify(checked.get(),options);
    Result result;result.elapsed=ns(end-begin);result.operations=options.iterations;result.layout_bytes=sizeof(pool);return result;
}
template<bool Padded>
Result stats_case(const Options& options,p::PerfControl& perf){
    auto checked=std::make_unique<Checked[]>(options.iterations);
    engine_labs::phase3::BasicScheduler<Padded> pool(options.threads,options.iterations,"one",[&](std::size_t i){execute(checked.get(),i,options);});
    if(options.pinned)pool.configure_workers([&](auto handle,std::size_t t){p::pin_native(handle,options.cpus[t%options.cpus.size()]);});
    const auto layout=pool.stats_layout();
    perf.enable();const auto begin=Clock::now();pool.resume();
    for(std::size_t i=0;i<options.iterations;++i)pool.submit(i,i%options.threads);
    const auto statistics=pool.join();const auto end=Clock::now();perf.disable();
    verify(checked.get(),options);std::size_t count=0;for(auto stat:statistics)count+=stat.tasks;p::require(count==options.iterations,"worker stats mismatch");
    Result result;result.elapsed=ns(end-begin);result.operations=options.iterations;result.layout_bytes=layout[0]*options.threads;return result;
}
void metric(const std::string& name,double value,const std::string& unit,const std::string& better="lower"){
    p::require(std::isfinite(value),"nonfinite metric");std::cout<<name<<','<<std::setprecision(17)<<value<<','<<unit<<','<<better<<",1,,0\n";
}
void layout_audit(){
    engine_labs::JobSystem baseline(1,false,true);
    engine_labs::BasicJobSystem<1> pending(1,false,true);
    engine_labs::BasicJobSystem<2> next(1,false,true);
    engine_labs::BasicJobSystem<3> wake(1,false,true);
    engine_labs::phase3::Scheduler stats(1,1,"static",[](std::size_t){});
    engine_labs::phase3::BasicScheduler<true> padded_stats(1,1,"static",[](std::size_t){});
    std::cout<<"{\"cache_line\":"<<p::cache_line_size()<<",\"cache_source\":\""
#ifdef _WIN32
      <<"GetLogicalProcessorInformationEx-RelationCache"
#else
      <<"sysfs-L1-data-coherency_line_size"
#endif
      <<"\",\"counter_offsets\":{";
    bool first=true;
    for(auto item:{std::make_pair("packed",baseline.counter_layout()),std::make_pair("pending",pending.counter_layout()),std::make_pair("next",next.counter_layout()),std::make_pair("wake",wake.counter_layout())}){
        if(!first)std::cout<<',';
        first=false;std::cout<<'\"'<<item.first<<"\":[";
        for(std::size_t i=0;i<item.second.size();++i){if(i)std::cout<<',';std::cout<<item.second[i];}std::cout<<']';
    }
    auto a=stats.stats_layout(),b=padded_stats.stats_layout();
    std::cout<<"},\"queue_metadata_bytes\":{\"packed\":"<<sizeof(engine_labs::phase3::BasicChaseLev<false>)<<",\"separated\":"<<sizeof(engine_labs::phase3::ChaseLev)
      <<"},\"stats_layout\":{\"packed\":["<<a[0]<<','<<a[1]<<','<<a[2]<<"],\"separated\":["<<b[0]<<','<<b[1]<<','<<b[2]<<"]},\"positive_stride\":{\"packed\":"<<sizeof(AtomicCell<std::uint64_t,false>)<<",\"separated\":"<<sizeof(AtomicCell<std::uint64_t,true>)<<"}}\n";
}
int main(int argc,char** argv){
 try{
    Args args(argc,argv);if(args.number("--inspect",0)){layout_audit();return 0;}
    const auto meta=w::verify_meta(args.get("--meta"));p::require(meta.at("kind").str()=="constructive","S8 requires constructive workload");
    if(!args.get("--fingerprint").empty())p::require(args.get("--fingerprint")==meta.at("workload_fingerprint").str(),"fingerprint mismatch");
    const auto& params=meta.at("descriptor").at("params");const auto scenario=params.at("case").str();const auto variant=args.get("--impl","packed");
    Options options;options.threads=args.number("--threads",4);options.iterations=params.at("iterations").u64();options.units=params.at("work_units").u64();options.pinned=args.number("--pin",0)!=0;
    p::require(options.threads>0&&options.threads<=256&&options.iterations>0&&options.iterations<=10000000&&options.units<=1000000,"unsupported experiment scale");
    options.cpus=p::available_cpus();
    if(!args.get("--cpus").empty()){
        std::vector<int> requested;std::stringstream stream(args.get("--cpus"));std::string part;
        while(std::getline(stream,part,','))requested.push_back(std::stoi(part));
        p::require(!requested.empty(),"empty CPU list");
        std::set<int> distinct(requested.begin(),requested.end());p::require(distinct.size()==requested.size(),"duplicate eligible CPU");
        p::restrict_eligibility(requested);options.cpus=requested;
    }
    p::require(p::cache_line_size()==64,"S8 compiled layout requires observed 64-byte lines");
    if(args.number("--require-ac",0))p::require(p::ac_power(),"AC power is required");
    p::PerfControl perf(args.get("--perf-control"),args.get("--perf-ack"));Result result;
    if(scenario=="positive"){
        if(variant=="packed")result=positive<false>(options,perf);else if(variant=="separated")result=positive<true>(options,perf);else throw std::invalid_argument("positive variant");
    }else if(scenario=="cold"){
        if(variant=="packed")result=negative<false>(options,perf);else if(variant=="separated")result=negative<true>(options,perf);else throw std::invalid_argument("cold variant");
    }else if(scenario=="queue"){
        if(variant=="packed")result=queue_case<false>(options,perf);else if(variant=="separated")result=queue_case<true>(options,perf);else throw std::invalid_argument("queue variant");
    }else if(scenario=="counters"){
        if(variant=="packed")result=counter_case<0>(options,perf);else if(variant=="pending")result=counter_case<1>(options,perf);else if(variant=="next")result=counter_case<2>(options,perf);else if(variant=="wake")result=counter_case<3>(options,perf);else throw std::invalid_argument("counter variant");
    }else if(scenario=="stats"){
        if(variant=="packed")result=stats_case<false>(options,perf);else if(variant=="separated")result=stats_case<true>(options,perf);else throw std::invalid_argument("stats variant");
    }else throw std::invalid_argument("unknown S8 case");
    std::cerr<<"S8_META:{\"eligibility_verified\":true,\"per_worker_binding_verified\":"<<(options.pinned?"true":"false")<<",\"eligible_cpus\":[";
    for(std::size_t i=0;i<options.cpus.size();++i){if(i)std::cerr<<',';std::cerr<<options.cpus[i];}
    std::cerr<<"],\"first_cpu\":[";for(std::size_t i=0;i<result.first.size();++i){if(i)std::cerr<<',';std::cerr<<result.first[i];}
    std::cerr<<"],\"last_cpu\":[";for(std::size_t i=0;i<result.last.size();++i){if(i)std::cerr<<',';std::cerr<<result.last[i];}std::cerr<<"]}\n";
    p::require(result.elapsed>0,"invalid duration");
    std::cout<<"metric,value,unit,better,valid,invalid_reason,stub\n";
    metric("throughput",double(result.operations)*1e9/result.elapsed,"items/s","higher");metric("elapsed_ns",result.elapsed,"ns");
    metric("operations",double(result.operations),"count");metric("selected_layout_bytes",double(result.layout_bytes),"bytes");
    metric("affinity_verified",1,"bool","higher");metric("cache_line_bytes",64,"bytes");
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::cout<<"metric,value,unit,better,valid,invalid_reason,stub\nvalidation,0,bool,higher,0,experiment_failed,0\n";return 1;}
}
