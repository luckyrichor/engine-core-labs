#include "pipeline.hpp"
int main(){
 auto root=std::filesystem::temp_directory_path()/"engine-l3-contract";
 std::filesystem::create_directories(root);auto file=root/"valid";
 {std::ofstream out(file,std::ios::binary);std::string bytes(65536,'x');out<<bytes;}
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
