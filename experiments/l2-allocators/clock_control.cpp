#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
int main(){
 using Clock=std::chrono::steady_clock;
 std::vector<double> pairs;pairs.reserve(100000);
 for(int i=0;i<100000;++i){auto a=Clock::now();auto b=Clock::now();pairs.push_back(std::chrono::duration<double,std::nano>(b-a).count());}
 std::sort(pairs.begin(),pairs.end());
 std::cout<<"samples,p50_clock_pair_ns,p99_clock_pair_ns,p50_amortized_256_ns\n"<<pairs.size()<<','<<pairs[pairs.size()/2]<<','<<pairs[pairs.size()*99/100]<<','<<pairs[pairs.size()/2]/256<<'\n';
}
