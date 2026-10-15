#include "pipeline.hpp"
#include <iostream>
#include <thread>
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=6)throw std::invalid_argument("usage: resource_bench MODE ASSET_DIR DECODE_ITERATIONS [WORKERS vector|arena]");
 std::string mode=argv[1];if(mode!="sync"&&mode!="async"&&mode!="preload")throw std::invalid_argument("mode");
 auto iterations=std::stoull(argv[3]);if(iterations>10000000)throw std::invalid_argument("decode bounds");
 auto workers=argc==6?std::stoull(argv[4]):4;
 std::string backend=argc==6?argv[5]:"vector";
 if(backend!="vector"&&backend!="arena")throw std::invalid_argument("backend");
 constexpr std::size_t assets=32,frames=96;
 auto path=[&](std::size_t id){return std::filesystem::path(argv[2])/(std::to_string(id)+".bin");};
 using Clock=std::chrono::steady_clock;
 auto ms=[](auto duration){return std::chrono::duration<double,std::milli>(duration).count();};
 auto start=Clock::now();
 resources::Pipeline pipeline(workers,8,iterations,backend=="arena");
 double setup=ms(Clock::now()-start);
 std::map<std::size_t,Clock::time_point> demand,accepted,installed_at;
 if(mode=="preload"){
  for(std::size_t id=0;id<assets;){
   demand.try_emplace(id,Clock::now());
   auto submitted=Clock::now();
   if(pipeline.request(path(id),id)){accepted.emplace(id,submitted);++id;}
   pipeline.commit(2);
  }
  pipeline.drain();
 }
 double startup=ms(Clock::now()-start);
 std::map<std::size_t,resources::Asset> installed;
 std::size_t requested=0,total_missing=0;
 std::cout<<"mode,workers,backend,frame,frame_ms,missing,in_flight,setup_ms,startup_ms,drain_ms,checksum,asset_id,demand_to_install_ms,request_to_install_ms\n";
 for(std::size_t frame=0;frame<frames;++frame){
  auto frame_start=Clock::now();
  if(frame%8==0&&requested<assets){
   auto previous=requested;requested=std::min(assets,requested+4);
   if(mode!="preload")for(auto id=previous;id<requested;++id)demand.emplace(id,frame_start);
  }
  if(mode=="sync")for(std::size_t id=0;id<requested;++id)if(!installed.contains(id)){
   accepted.emplace(id,Clock::now());installed.emplace(id,resources::load(path(id),id,iterations));installed_at.emplace(id,Clock::now());
  }
  if(mode=="async")for(std::size_t id=0;id<requested;++id)if(!pipeline.contains(id)){
   auto submitted=Clock::now();if(pipeline.request(path(id),id))accepted.try_emplace(id,submitted);
  }
  if(mode!="sync")pipeline.commit(2);
  std::size_t missing=0;for(std::size_t id=0;id<requested;++id)if(mode=="sync"?!installed.contains(id):!pipeline.contains(id))++missing;
  total_missing+=missing;
  std::this_thread::sleep_for(std::chrono::microseconds(200));
  auto frame_ms=ms(Clock::now()-frame_start);
  std::cout<<mode<<','<<workers<<','<<backend<<','<<frame<<','<<frame_ms<<','<<missing<<','<<pipeline.in_flight()<<','<<setup<<','<<startup<<",0,0,-1,0,0\n";
  std::this_thread::sleep_until(frame_start+std::chrono::milliseconds(5));
 }
 auto drain_start=Clock::now();pipeline.drain();auto drain=ms(Clock::now()-drain_start);
 std::uint64_t checksum=0,expected=0;
 for(std::size_t id=0;id<assets;++id){
  expected+=resources::load(path(id),id,iterations).hash;
  if(mode=="sync")checksum+=installed.at(id).hash;
  else{if(!pipeline.contains(id))throw std::runtime_error("missing final asset");installed_at.emplace(id,pipeline.installed_at(id));}
 }
 if(mode!="sync")checksum=pipeline.checksum();
 if(checksum!=expected)throw std::runtime_error("checksum mismatch");
 for(std::size_t id=0;id<assets;++id){
  auto end=installed_at.at(id);
  if(end<accepted.at(id)||accepted.at(id)<demand.at(id))throw std::runtime_error("latency order");
  std::cout<<mode<<','<<workers<<','<<backend<<",-2,0,0,0,"<<setup<<','<<startup<<",0,0,"<<id<<','<<ms(end-demand.at(id))<<','<<ms(end-accepted.at(id))<<'\n';
 }
 std::cout<<mode<<','<<workers<<','<<backend<<",-1,0,"<<total_missing<<",0,"<<setup<<','<<startup<<','<<drain<<','<<checksum<<",-1,0,0\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
