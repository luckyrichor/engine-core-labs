#include "pipeline.hpp"
#include <iostream>
#include <thread>
int main(int argc,char**argv){try{
 if(argc!=4)throw std::invalid_argument("usage: resource_bench MODE ASSET_DIR DECODE_ITERATIONS");
 std::string mode=argv[1];if(mode!="sync"&&mode!="async"&&mode!="preload")throw std::invalid_argument("mode");
 auto iterations=std::stoull(argv[3]);if(iterations>10000000)throw std::invalid_argument("decode bounds");
 constexpr std::size_t assets=32,frames=96;
 auto path=[&](std::size_t id){return std::filesystem::path(argv[2])/(std::to_string(id)+".bin");};
 resources::Pipeline pipeline(4,8,iterations);
 using Clock=std::chrono::steady_clock;
 auto start=Clock::now();
 if(mode=="preload")for(std::size_t id=0;id<assets;){if(pipeline.request(path(id),id))++id;pipeline.commit(2);}
 if(mode=="preload")pipeline.drain();
 double startup=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 std::map<std::size_t,resources::Asset> installed;
 std::size_t requested=0,total_missing=0;
 std::cout<<"mode,frame,frame_ms,missing,in_flight,startup_ms,drain_ms,checksum\n";
 for(std::size_t frame=0;frame<frames;++frame){
  auto frame_start=Clock::now();
  if(frame%8==0&&requested<assets)requested=std::min(assets,requested+4);
  if(mode=="sync"){for(std::size_t id=0;id<requested;++id)if(!installed.contains(id))installed.emplace(id,resources::load(path(id),id,iterations));}
  if(mode=="async")for(std::size_t id=0;id<requested;++id)if(!pipeline.contains(id))pipeline.request(path(id),id);
  if(mode!="sync")pipeline.commit(2);
  std::size_t missing=0;for(std::size_t id=0;id<requested;++id)if(mode=="sync"?!installed.contains(id):!pipeline.contains(id))++missing;
  total_missing+=missing;
  // Simulated non-loading frame work, included equally in all modes.
  std::this_thread::sleep_for(std::chrono::microseconds(200));
  auto frame_ms=std::chrono::duration<double,std::milli>(Clock::now()-frame_start).count();
  std::cout<<mode<<','<<frame<<','<<frame_ms<<','<<missing<<','<<pipeline.in_flight()<<','<<startup<<",0,0\n";
  std::this_thread::sleep_until(frame_start+std::chrono::milliseconds(5));
 }
 auto drain_start=Clock::now();pipeline.drain();auto drain=std::chrono::duration<double,std::milli>(Clock::now()-drain_start).count();
 std::uint64_t checksum=0,expected=0;
 for(std::size_t id=0;id<assets;++id){expected+=resources::load(path(id),id,iterations).hash;if(mode=="sync")checksum+=installed.at(id).hash;else if(!pipeline.contains(id))throw std::runtime_error("missing final asset");}
 if(mode!="sync"){checksum=pipeline.checksum();}
 if(checksum!=expected)throw std::runtime_error("checksum mismatch");
 std::cout<<mode<<",-1,0,"<<total_missing<<",0,"<<startup<<','<<drain<<','<<checksum<<'\n';
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
