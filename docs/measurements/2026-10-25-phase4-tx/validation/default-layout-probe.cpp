#include "job_system.hpp"
#include "scheduler.hpp"
#include <iostream>
int main(){std::cout<<"{\"job_bytes\":"<<sizeof(engine_labs::JobSystem)<<",\"job_alignment\":"<<alignof(engine_labs::JobSystem)<<",\"chase_bytes\":"<<sizeof(engine_labs::phase3::ChaseLev)<<",\"chase_alignment\":"<<alignof(engine_labs::phase3::ChaseLev)<<",\"stats_bytes\":"<<sizeof(engine_labs::phase3::WorkerStats)<<"}\n";}
