// Independent answer oracle. No experiment headers, ufbx, or measured broad-phase code.
// Uniform grid with unique-cell ownership; oversized boxes query static BVHs.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <vector>
using Box=std::array<float,6>;
using Cell=std::array<std::int64_t,3>;
struct Range { Cell lo,hi; bool large=false; };
bool overlap(const Box& a,const Box& b) {
    for(int k=0;k<3;++k) if(a[k]>b[k+3] || b[k]>a[k+3]) return false;
    return true;
}
Box bounds(const std::vector<Box>& b,const std::vector<std::uint32_t>& ids,std::size_t lo,std::size_t hi) {
    Box a=b[ids[lo]];
    for(auto i=lo+1;i<hi;++i) for(int k=0;k<3;++k) {
        a[k]=std::min(a[k],b[ids[i]][k]); a[k+3]=std::max(a[k+3],b[ids[i]][k+3]);
    }
    return a;
}
struct Tree {
    struct Node { Box box; std::size_t begin,end; int left=-1,right=-1; std::uint32_t min_id; };
    const std::vector<Box>& boxes;
    std::vector<std::uint32_t> ids;
    std::vector<Node> nodes;
    Tree(const std::vector<Box>& b,std::vector<std::uint32_t> v):boxes(b),ids(std::move(v)) {
        nodes.reserve(ids.size()/4+1); if(!ids.empty()) build(0,ids.size());
    }
    int build(std::size_t lo,std::size_t hi) {
        auto a=bounds(boxes,ids,lo,hi);
        auto min_id=*std::min_element(ids.begin()+lo,ids.begin()+hi);
        int index=static_cast<int>(nodes.size()); nodes.push_back({a,lo,hi,-1,-1,min_id});
        if(hi-lo>16) {
            int axis=0; for(int k=1;k<3;++k) if(double(a[k+3])-a[k]>double(a[axis+3])-a[axis]) axis=k;
            auto mid=lo+(hi-lo)/2;
            std::nth_element(ids.begin()+lo,ids.begin()+mid,ids.begin()+hi,[&](auto i,auto j) {
                double x=double(boxes[i][axis])+boxes[i][axis+3], y=double(boxes[j][axis])+boxes[j][axis+3];
                return x==y ? i<j : x<y;
            });
            int l=build(lo,mid),r=build(mid,hi); nodes[index].left=l; nodes[index].right=r;
        }
        return index;
    }
    std::uint64_t query(const Box& b,std::uint32_t before,int index=0) const {
        if(nodes.empty()) return 0;
        const auto& n=nodes[index];
        if(n.min_id>=before || !overlap(b,n.box)) return 0;
        if(n.left>=0) return query(b,before,n.left)+query(b,before,n.right);
        std::uint64_t count=0;
        for(auto p=n.begin;p<n.end;++p) if(ids[p]<before && overlap(b,boxes[ids[p]])) ++count;
        return count;
    }
};
struct Hash {
    std::size_t operator()(const Cell& c) const {
        std::uint64_t h=0;
        for(auto v:c) { auto x=std::uint64_t(v)+0x9e3779b97f4a7c15ULL; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
            x=(x^(x>>27))*0x94d049bb133111ebULL; h^=(x^(x>>31))+(h<<6)+(h>>2); }
        return static_cast<std::size_t>(h);
    }
};
std::vector<Box> read(const char* path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) throw std::runtime_error("cannot open AABB input");
    auto length=in.tellg();
    if(length<0 || length%24!=0 || std::uint64_t(length/24)>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("invalid AABB file length");
    std::vector<Box> boxes(static_cast<std::size_t>(length/24)); in.seekg(0);
    for(auto& b:boxes) for(float& f:b) {
        unsigned char bytes[4]; in.read(reinterpret_cast<char*>(bytes),4);
        std::uint32_t v=0; for(int k=0;k<4;++k) v|=std::uint32_t(bytes[k])<<(8*k);
        f=std::bit_cast<float>(v); if(!std::isfinite(f)) throw std::runtime_error("nonfinite AABB");
    }
    if(!in) throw std::runtime_error("truncated AABB input");
    for(const auto& b:boxes) for(int k=0;k<3;++k) if(b[k]>b[k+3]) throw std::runtime_error("inverted AABB");
    return boxes;
}
double choose_size(const std::vector<Box>& boxes) {
    double chosen=0;
    std::vector<double> widths; widths.reserve(boxes.size());
    for(int k=0;k<3;++k) {
        widths.clear(); for(const auto& b:boxes) widths.push_back(double(b[k+3])-b[k]);
        if(widths.empty()) continue;
        auto mid=widths.begin()+widths.size()/2; std::nth_element(widths.begin(),mid,widths.end());
        chosen=std::max(chosen,*mid);
    }
    return chosen>0 ? chosen*4 : 1;
}
int main(int argc,char** argv) {
    try {
        if(argc<2 || argc>3) throw std::runtime_error("usage: aabb_reference input.bin [cell_size]");
        auto boxes=read(argv[1]); double size=argc==3 ? std::stod(argv[2]) : choose_size(boxes);
        if(!std::isfinite(size)||size<=0) throw std::runtime_error("invalid cell size");
        std::vector<Range> ranges(boxes.size()); std::vector<std::uint32_t> big,all(boxes.size());
        std::iota(all.begin(),all.end(),0);
        constexpr std::uint64_t limit=512;
        for(std::size_t i=0;i<boxes.size();++i) {
            auto& r=ranges[i]; std::uint64_t cells=1;
            for(int k=0;k<3;++k) {
                double lo=std::floor(double(boxes[i][k])/size),hi=std::floor(double(boxes[i][k+3])/size);
                // Guard integer conversion and products; outliers go through bounded-memory trees.
                if(lo < -1e15 || hi > 1e15) { r.large=true; continue; }
                r.lo[k]=static_cast<std::int64_t>(lo); r.hi[k]=static_cast<std::int64_t>(hi);
                auto span=std::uint64_t(r.hi[k]-r.lo[k]+1);
                if(span>limit || cells>limit/span) r.large=true;
                else cells*=span;
            }
            if(r.large) big.push_back(static_cast<std::uint32_t>(i));
        }
        Tree large_tree(boxes,big);
        // Build the full tree only when an oversized box actually needs it.
        Tree full_tree(boxes,big.empty()?std::vector<std::uint32_t>{}:std::move(all));
        std::unordered_map<Cell,std::vector<std::uint32_t>,Hash> grid;
        grid.reserve(boxes.size()); std::uint64_t answer=0;
        for(std::uint32_t i=0;i<boxes.size();++i) {
            const auto& r=ranges[i]; const auto& b=boxes[i];
            if(r.large) { answer+=full_tree.query(b,i); continue; }
            answer+=large_tree.query(b,i);
            for(auto x=r.lo[0];x<=r.hi[0];++x) for(auto y=r.lo[1];y<=r.hi[1];++y) for(auto z=r.lo[2];z<=r.hi[2];++z) {
                Cell key{x,y,z}; auto& bucket=grid[key];
                for(auto j:bucket) {
                    // Count at only the lexicographically first shared cell; no per-query candidate set.
                    const auto& other=ranges[j];
                    if(x==std::max(r.lo[0],other.lo[0]) && y==std::max(r.lo[1],other.lo[1]) &&
                       z==std::max(r.lo[2],other.lo[2]) && overlap(b,boxes[j])) ++answer;
                }
                bucket.push_back(i);
            }
        }
        std::cout << std::setprecision(17) << "{\"overlap_pairs\":" << answer << ",\"object_count\":" << boxes.size()
                  << ",\"cell_size\":" << size << ",\"cell_size_rule\":\"4*max_axis_upper_median_extent; all_zero=1\","
                  << "\"max_cells_per_box\":512,\"oversized_boxes\":" << big.size()
                  << ",\"oversized_fallback\":\"static_median_bvh_leaf16\",\"answer_method\":\"independent_cpp_grid_bvh\"}\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
