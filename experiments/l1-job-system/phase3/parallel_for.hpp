#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
namespace engine_labs::phase3 {
// Core-independent blocked traversal. block=1 is exactly element submission.
template<class Submit,class F>
void parallel_for(std::size_t n,std::size_t block,Submit submit,F function) {
    if(!block)throw std::invalid_argument("block must be nonzero");
    for(std::size_t begin=0;begin<n;) {
        const auto end=begin+std::min(block,n-begin);
        submit([=]{for(auto i=begin;i<end;++i)function(i);});
        begin=end;
    }
}
}
