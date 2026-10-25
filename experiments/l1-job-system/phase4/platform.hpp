#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif
namespace engine_labs::phase4 {
inline void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
inline std::vector<int> available_cpus(){
    std::vector<int> cpus;
#ifdef _WIN32
    DWORD_PTR process=0,system=0;
    require(GetProcessAffinityMask(GetCurrentProcess(),&process,&system)!=0,"GetProcessAffinityMask failed");
    for(int cpu=0;cpu<64;++cpu)if(process&(DWORD_PTR{1}<<cpu))cpus.push_back(cpu);
#else
    cpu_set_t set;CPU_ZERO(&set);
    require(sched_getaffinity(0,sizeof(set),&set)==0,"sched_getaffinity failed");
    for(int cpu=0;cpu<CPU_SETSIZE;++cpu)if(CPU_ISSET(cpu,&set))cpus.push_back(cpu);
#endif
    require(!cpus.empty(),"empty CPU eligibility");return cpus;
}
inline void restrict_eligibility(const std::vector<int>& cpus){
    const auto allowed=available_cpus();
    for(int cpu:cpus)require(std::find(allowed.begin(),allowed.end(),cpu)!=allowed.end(),"requested CPU not eligible");
#ifdef _WIN32
    DWORD_PTR mask=0;
    for(int cpu:cpus){require(cpu>=0&&cpu<64,"process mask supports group 0 only");mask|=DWORD_PTR{1}<<cpu;}
    require(SetProcessAffinityMask(GetCurrentProcess(),mask)!=0,"SetProcessAffinityMask failed");
#else
    cpu_set_t set;CPU_ZERO(&set);for(int cpu:cpus)CPU_SET(cpu,&set);
    require(sched_setaffinity(0,sizeof(set),&set)==0,"sched_setaffinity eligibility failed");
#endif
    auto expected=cpus;std::sort(expected.begin(),expected.end());
    require(available_cpus()==expected,"CPU eligibility readback mismatch");
}
inline void pin_native(std::thread::native_handle_type handle,int cpu){
#ifdef _WIN32
    GROUP_AFFINITY affinity{};affinity.Group=static_cast<WORD>(cpu/64);affinity.Mask=KAFFINITY{1}<<(cpu%64);
    require(SetThreadGroupAffinity(handle,&affinity,nullptr)!=0,"SetThreadGroupAffinity failed");
    GROUP_AFFINITY read{};require(GetThreadGroupAffinity(handle,&read)!=0,"GetThreadGroupAffinity failed");
    require(read.Group==affinity.Group&&read.Mask==affinity.Mask,"thread affinity readback mismatch");
#else
    cpu_set_t set;CPU_ZERO(&set);require(cpu>=0&&cpu<CPU_SETSIZE,"CPU out of mask range");CPU_SET(cpu,&set);
    require(pthread_setaffinity_np(handle,sizeof(set),&set)==0,"pthread_setaffinity_np failed");
    cpu_set_t read;CPU_ZERO(&read);require(pthread_getaffinity_np(handle,sizeof(read),&read)==0,"pthread_getaffinity_np failed");
    require(CPU_COUNT(&read)==1&&CPU_ISSET(cpu,&read),"thread affinity readback mismatch");
#endif
}
inline void pin_current(int cpu){
#ifdef _WIN32
    pin_native(GetCurrentThread(),cpu);
#else
    cpu_set_t set;CPU_ZERO(&set);require(cpu>=0&&cpu<CPU_SETSIZE,"CPU out of mask range");CPU_SET(cpu,&set);
    require(sched_setaffinity(0,sizeof(set),&set)==0,"sched_setaffinity worker failed");
    const auto read=available_cpus();require(read.size()==1&&read.front()==cpu,"worker affinity readback mismatch");
#endif
}
inline int current_cpu(){
#ifdef _WIN32
    PROCESSOR_NUMBER number{};GetCurrentProcessorNumberEx(&number);return number.Group*64+number.Number;
#else
    return sched_getcpu();
#endif
}
inline bool ac_power(){
#ifdef _WIN32
    SYSTEM_POWER_STATUS status{};require(GetSystemPowerStatus(&status)!=0,"GetSystemPowerStatus failed");return status.ACLineStatus==1;
#else
    return true;
#endif
}
inline std::size_t cache_line_size(){
    std::set<std::size_t> lines;
#ifdef _WIN32
    DWORD size=0;GetLogicalProcessorInformationEx(RelationCache,nullptr,&size);
    require(size>0,"cache topology size unavailable");std::vector<unsigned char> bytes(size);
    require(GetLogicalProcessorInformationEx(RelationCache,reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(bytes.data()),&size)!=0,"cache topology read failed");
    for(std::size_t offset=0;offset<size;){
        auto* record=reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(bytes.data()+offset);
        require(record->Size>=8&&offset+record->Size<=size,"invalid cache topology");
        if(record->Relationship==RelationCache && record->Cache.Level==1 && (record->Cache.Type==CacheData || record->Cache.Type==CacheUnified))lines.insert(record->Cache.LineSize);
        offset+=record->Size;
    }
#else
    for(int cpu:available_cpus())for(int index=0;index<10;++index){
        const auto base="/sys/devices/system/cpu/cpu"+std::to_string(cpu)+"/cache/index"+std::to_string(index)+"/";
        std::ifstream level_file(base+"level"),type_file(base+"type"),line_file(base+"coherency_line_size");
        int level=0;std::string type;std::size_t line=0;
        if(level_file>>level && type_file>>type && line_file>>line && level==1 && (type=="Data"||type=="Unified"))lines.insert(line);
    }
#endif
    require(lines.size()==1,"cache line size not uniquely confirmed");return *lines.begin();
}
// perf stat starts disabled and acknowledges enable/disable. Affinity and setup
// are completed before enabling; the counters exclude validation and teardown.
class PerfControl {
#ifndef _WIN32
    int control_{-1},ack_{-1};
    void send(const std::string& command){
        if(control_<0)return;
        const std::string text=command+"\n";
        require(write(control_,text.data(),text.size())==static_cast<ssize_t>(text.size()),"perf control write failed");
        std::string response;
        while(response.find('\n')==std::string::npos){
            pollfd descriptor{ack_,POLLIN,0};require(poll(&descriptor,1,10000)>0,"perf acknowledgement timeout");
            char buffer[32];const auto count=read(ack_,buffer,sizeof(buffer));require(count>0,"perf acknowledgement read failed");response.append(buffer,static_cast<std::size_t>(count));
        }
        // perf's evlist__ctlfd_ack writes sizeof("ack\n"), including NUL.
        response.erase(std::remove(response.begin(),response.end(),'\0'),response.end());
        if(response!="ack\n"){std::string bytes;for(unsigned char c:response)bytes+=std::to_string(c)+",";throw std::runtime_error("unexpected perf acknowledgement bytes="+bytes);}
    }
#endif
public:
    PerfControl(const std::string& control,const std::string& ack){
#ifndef _WIN32
        if(!control.empty()){
            require(!ack.empty(),"missing perf acknowledgement FIFO");
            control_=open(control.c_str(),O_WRONLY);require(control_>=0,"cannot open perf control FIFO");
            ack_=open(ack.c_str(),O_RDONLY|O_NONBLOCK);require(ack_>=0,"cannot open perf acknowledgement FIFO");
        }
#else
        require(control.empty()&&ack.empty(),"perf control is Linux-only");
#endif
    }
    ~PerfControl(){
#ifndef _WIN32
        if(control_>=0)close(control_);
        if(ack_>=0)close(ack_);
#endif
    }
    void enable(){
#ifndef _WIN32
        send("enable");
#endif
    }
    void disable(){
#ifndef _WIN32
        send("disable");
#endif
    }
};
}
