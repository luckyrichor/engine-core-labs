#include "variable_heap.hpp"
#include <iostream>
int main(){
 allocators::VariableHeap heap;std::vector<void*> blocks;
 for(int i=0;i<512;++i)blocks.push_back(heap.allocate(128));
 for(int i=0;i<512;i+=2)heap.release(blocks[i]);
 auto [free,largest]=heap.free_extent();bool failed=false;
 try{heap.allocate(256);}catch(const std::bad_alloc&){failed=true;}
 if(!failed||free!=32768||largest!=128)throw std::runtime_error("fragmentation scenario changed");
 std::cout<<"phase,total_free,largest_free,requested,can_allocate,external_fragmentation\n"
 <<"alternating_holes,"<<free<<','<<largest<<",256,false,"<<1.0-double(largest)/free<<'\n';
 for(int i=1;i<512;i+=2)heap.release(blocks[i]);
 auto [merged,best]=heap.free_extent();auto p=heap.allocate(256);heap.release(p);
 if(merged!=allocators::capacity||best!=merged)throw std::runtime_error("coalescing failed");
 std::cout<<"coalesced,"<<merged<<','<<best<<",256,true,0\n";
}
