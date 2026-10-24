#include "scheduler.hpp"
#include "parallel_for.hpp"
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
using namespace engine_labs::phase3;
void require(bool ok){if(!ok)throw std::runtime_error("phase3 contract failed");}
int main(){
    // Ring wrap, empty pop, last-element races, multiple thieves, bounded full.
    ChaseLev queue(7);
    require(!queue.pop());
    for(std::size_t i=0;i<8;++i)require(queue.push(i));
    require(!queue.push(99));
    for(int i=7;i>=0;--i)require(queue.pop()==std::size_t(i));
    for(std::size_t i=0;i<10000;++i){require(queue.push(i));require(queue.steal()==i);require(!queue.pop());}
    for(int round=0;round<12;++round){
        constexpr std::size_t n=20000;
        ChaseLev ring(63);
        std::vector<std::atomic<int>> seen(n);
        std::atomic<bool> end{false};
        std::vector<std::thread> thieves;
        for(int t=0;t<4;++t)thieves.emplace_back([&]{while(!end.load()){if(auto id=ring.steal()){require(*id<n);++seen[*id];}else std::this_thread::yield();}while(auto id=ring.steal())++seen[*id];});
        for(std::size_t i=0;i<n;++i){while(!ring.push(i)){if(auto id=ring.pop())++seen[*id];}if(i%3==0)if(auto id=ring.pop())++seen[*id];}
        while(auto id=ring.pop())++seen[*id];
        end.store(true);
        for(auto& t:thieves)t.join();
        for(auto& value:seen)require(value.load()==1);
    }
    for(const std::string policy:{"static","one","half","chase"})for(bool prequeued:{true,false})for(int round=0;round<5;++round){
        constexpr std::size_t n=4000;std::vector<std::atomic<int>> seen(n);
        Scheduler pool(4,n,policy,[&](std::size_t id){++seen[id];});
        if(!prequeued)pool.resume();
        std::vector<std::thread> producers;
        for(std::size_t p=0;p<4;++p)producers.emplace_back([&,p]{for(std::size_t i=p;i<n;i+=4)pool.submit(i,round%2 ? i%4 : 0);});
        for(auto& p:producers)p.join();
        if(prequeued){for(auto& v:seen)require(v.load()==0);pool.resume();}
        const auto stats=pool.join();std::uint64_t sum=0;for(auto s:stats)sum+=s.tasks;require(sum==n);
        for(auto& v:seen)require(v.load()==1);
    }
    for(std::size_t block:{1,3,64,1000}){
        std::vector<int> seen(100);std::size_t tasks=0;
        parallel_for(seen.size(),block,[&](auto f){++tasks;f();},[&](auto i){++seen[i];});
        require(tasks==(seen.size()+block-1)/block);for(auto v:seen)require(v==1);
    }
    std::cout<<"Chase-Lev wrap/last-item/4 thieves; 4 producers/all policies/two start modes; blocked traversal: exactly once\n";
}
