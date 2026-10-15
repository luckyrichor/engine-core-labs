#include "pipeline.hpp"
int main(){
 auto root=std::filesystem::temp_directory_path()/"engine-l3-contract";
 std::filesystem::create_directories(root);auto file=root/"valid";
 {std::ofstream out(file,std::ios::binary);std::string bytes(65536,'x');out<<bytes;}
 // Exercise overlapping scratch leases, exhaustion, reuse and payload isolation.
 resources::Buffers buffers(2);
 {auto first=buffers.acquire();auto second=buffers.acquire();
  if(first.data==second.data)throw std::runtime_error("overlapping buffers");
  first.data[65535]=42;second.data[65535]=17;
  bool exhausted=false;try{auto extra=buffers.acquire();}catch(const std::runtime_error&){exhausted=true;}
  if(!exhausted||first.data[65535]!=42)throw std::runtime_error("buffer bounds");
 }
 {auto reused=buffers.acquire();reused.data[0]=1;}
 for(bool reuse:{false,true}){
 resources::Pipeline parallel(4,8,10,reuse);
 for(std::size_t i=0;i<32;){if(parallel.request(file,i))++i;parallel.commit(2);}
 parallel.drain();
 if(parallel.checksum()!=32*resources::load(file,0,10).hash)throw std::runtime_error("scratch checksum");
 resources::Pipeline failed(1,1,0,reuse);failed.request(root/"absent",0);
 bool failure=false;try{failed.drain();}catch(const std::runtime_error&){failure=true;}
 if(!failure||failed.contains(0))throw std::runtime_error("failed install");
 // Failed decode must release scratch capacity too.
 failed.request(file,1);failed.drain();
 if(!failed.contains(1))throw std::runtime_error("lease leak on exception");
 }
 resources::Pipeline p(2,1,10);
 if(!p.request(file,1)||!p.request(file,1)||p.request(file,2))throw std::runtime_error("backpressure/dedup");
 p.drain();if(!p.contains(1)||p.in_flight()!=0)throw std::runtime_error("commit");
 if(!p.request(file,2))throw std::runtime_error("capacity release");
 p.drain();
 resources::Pipeline bad(1,1,0);bad.request(root/"absent",0);bool threw=false;
 try{bad.drain();}catch(const std::runtime_error&){threw=true;}
 if(!threw||bad.contains(0))throw std::runtime_error("error propagation");
 std::filesystem::remove_all(root);
}
