#pragma once
// ECL workload v1: standard-library-only JSON/meta verification and packed LE IO.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace ecl::workload {
inline void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
inline std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(bool(in), "cannot open workload file");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
inline std::string sha256(const std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint32_t,64> k = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> h = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto data = bytes;
    const auto bits = std::uint64_t(data.size()) * 8;
    data.push_back(0x80);
    while (data.size()%64 != 56) data.push_back(0);
    for (int i=7; i>=0; --i) data.push_back(std::uint8_t(bits >> (i*8)));
    for (std::size_t offset=0; offset<data.size(); offset+=64) {
        std::array<std::uint32_t,64> w{};
        for (int i=0;i<16;++i)
            for (int j=0;j<4;++j) w[i]=(w[i]<<8)|data[offset+std::size_t(i*4+j)];
        for (int i=16;i<64;++i) {
            const auto a=w[i-15], b=w[i-2];
            w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]
                 +(std::rotr(b,17)^std::rotr(b,19)^(b>>10));
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(int i=0;i<64;++i) {
            const auto t1=v+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))
                         +((e&f)^(~e&g))+k[i]+w[i];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for(auto word:h) out << std::setw(8) << word;
    return out.str();
}

// Minimal JSON value parser; numeric lexemes retained to preserve uint64 answers.
struct Json {
    enum class Type {object, array, string, number, literal};
    Type type=Type::literal;
    std::string value;
    std::map<std::string,Json> object;
    std::vector<Json> array;
    const Json& at(const std::string& key) const { return object.at(key); }
    std::uint64_t u64() const {
        require(type==Type::number && !value.empty() && value.find_first_not_of("0123456789")==std::string::npos,
                "expected unsigned JSON integer");
        return std::stoull(value);
    }
    const std::string& str() const { require(type==Type::string,"expected JSON string"); return value; }
};
class JsonParser {
    std::string text;
    std::size_t pos=0;
    void ws() { while(pos<text.size() && (text[pos]==' '||text[pos]=='\n'||text[pos]=='\r'||text[pos]=='\t')) ++pos; }
    char take() { require(pos<text.size(),"truncated JSON"); return text[pos++]; }
    unsigned hex4() {
        unsigned v=0;
        for(int i=0;i<4;++i) {
            auto c=take(); unsigned d;
            if(c>='0'&&c<='9') d=unsigned(c-'0');
            else if(c>='a'&&c<='f') d=unsigned(c-'a'+10);
            else if(c>='A'&&c<='F') d=unsigned(c-'A'+10);
            else throw std::runtime_error("invalid JSON escape");
            v=v*16+d;
        }
        return v;
    }
    std::string string() {
        require(take()=='"',"expected JSON quote");
        std::string out;
        while(true) {
            char c=take(); if(c=='"') return out;
            require(static_cast<unsigned char>(c)>=32,"JSON control character");
            if(c!='\\') {out+=c;continue;}
            c=take();
            switch(c) {
            case '"':case '\\':case '/':out+=c;break;
            case 'b':out+='\b';break;case 'f':out+='\f';break;
            case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
            case 'u': {
                unsigned cp=hex4();
                if(cp>=0xd800&&cp<=0xdbff) {
                    require(take()=='\\' && take()=='u',"invalid surrogate");
                    auto low=hex4(); require(low>=0xdc00&&low<=0xdfff,"invalid surrogate");
                    cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
                } else require(cp<0xdc00||cp>0xdfff,"invalid surrogate");
                if(cp<128) out+=char(cp);
                else {
                    if(cp>=0x10000) out+=char(0xf0|(cp>>18));
                    if(cp>=0x800) out+=char((cp<0x10000?0xe0:0x80)|((cp>>12)&63));
                    out+=char((cp<0x800?0xc0:0x80)|((cp>>6)&63));out+=char(0x80|(cp&63));
                }
                break;
            }
            default:throw std::runtime_error("invalid JSON escape");
            }
        }
    }
    Json parse_value(unsigned depth) {
        require(depth<128,"JSON nesting too deep"); ws();
        require(pos<text.size(),"empty JSON"); Json j;
        char c=text[pos];
        if(c=='"') {j.type=Json::Type::string;j.value=string();return j;}
        if(c=='{'||c=='[') {
            ++pos; const bool obj=c=='{'; const char close=obj?'}':']';
            j.type=obj?Json::Type::object:Json::Type::array;ws();
            if(pos<text.size()&&text[pos]==close) {++pos;return j;}
            while(true) {
                ws();std::string key;
                if(obj) {key=string();ws();require(take()==':',"expected JSON colon");}
                auto v=parse_value(depth+1);
                if(obj) require(j.object.emplace(key,std::move(v)).second,"duplicate JSON key");
                else j.array.push_back(std::move(v));
                ws();char end=take();if(end==close) return j;
                require(end==',',"expected JSON comma");
            }
        }
        auto start=pos;
        while(pos<text.size()&&std::string(" \r\n\t,]}").find(text[pos])==std::string::npos) ++pos;
        j.value=text.substr(start,pos-start);require(!j.value.empty(),"invalid JSON value");
        if(j.value=="null"||j.value=="true"||j.value=="false") return j;
        // Validate JSON number grammar without conversion or locale dependence.
        std::size_t n=0;const auto& s=j.value;
        if(s[n]=='-') ++n;
        require(n<s.size(),"invalid JSON number");
        if(s[n]=='0') ++n;
        else {require(s[n]>='1'&&s[n]<='9',"invalid JSON number");while(n<s.size()&&s[n]>='0'&&s[n]<='9') ++n;}
        if(n<s.size()&&s[n]=='.') {auto a=++n;while(n<s.size()&&s[n]>='0'&&s[n]<='9')++n;require(n>a,"invalid JSON fraction");}
        if(n<s.size()&&(s[n]=='e'||s[n]=='E')) {++n;if(n<s.size()&&(s[n]=='+'||s[n]=='-'))++n;auto a=n;while(n<s.size()&&s[n]>='0'&&s[n]<='9')++n;require(n>a,"invalid JSON exponent");}
        require(n==s.size(),"invalid JSON number");j.type=Json::Type::number;return j;
    }
public:
    explicit JsonParser(std::string s):text(std::move(s)){}
    Json parse() {auto j=parse_value(0);ws();require(pos==text.size(),"trailing JSON");return j;}
};
inline Json read_meta(const std::filesystem::path& path) {
    auto bytes=read_bytes(path);
    auto meta=JsonParser(std::string(bytes.begin(),bytes.end())).parse();
    require(meta.at("format_version").u64()==1,"unsupported workload version");
    require(meta.at("files").type==Json::Type::object,"expected files object");
    return meta;
}
inline std::vector<std::uint8_t> verified_file(const std::filesystem::path& meta_path,
                                            const Json& meta, const std::string& name) {
    const auto relative=std::filesystem::path(std::u8string(name.begin(),name.end()));
    require(!relative.empty()&&!relative.is_absolute()&&!relative.has_parent_path(),"unsafe data filename");
    auto data=read_bytes(meta_path.parent_path()/relative);
    require(sha256(data)==meta.at("files").at(name).str(),"workload SHA256 mismatch");
    return data;
}
inline Json verify_meta(const std::filesystem::path& path) {
    auto meta=read_meta(path);
    for(const auto& [name,unused]:meta.at("files").object) (void)verified_file(path,meta,name);
    return meta;
}
inline std::uint64_t le(const std::vector<std::uint8_t>& data, std::size_t offset, unsigned size) {
    require(size<=8&&offset<=data.size()&&size<=data.size()-offset,"truncated record");
    std::uint64_t n=0;for(unsigned i=0;i<size;++i)n|=std::uint64_t(data[offset+i])<<(i*8);return n;
}
inline std::vector<std::uint32_t> read_task_costs(const std::vector<std::uint8_t>& bytes) {
    require(bytes.size()%4==0,"truncated task record");std::vector<std::uint32_t> out;
    for(std::size_t i=0;i<bytes.size();i+=4) out.push_back(std::uint32_t(le(bytes,i,4)));
    return out;
}
using Aabb=std::array<float,6>;
inline std::vector<Aabb> read_aabbs(const std::vector<std::uint8_t>& bytes) {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
    require(bytes.size()%24==0,"truncated AABB record");std::vector<Aabb> out;
    for(std::size_t i=0;i<bytes.size();i+=24) {
        Aabb box{};
        for(unsigned k=0;k<6;++k) {box[k]=std::bit_cast<float>(std::uint32_t(le(bytes,i+k*4,4)));require(std::isfinite(box[k]),"nonfinite AABB");}
        for(unsigned k=0;k<3;++k) require(box[k]<=box[k+3],"inverted AABB");
        out.push_back(box);
    }return out;
}
struct TraceRecord { std::uint64_t timestamp,size;std::uint32_t thread;std::uint8_t operation;std::uint64_t object_id; };
inline std::vector<TraceRecord> read_trace(const std::vector<std::uint8_t>& bytes) {
    require(bytes.size()%29==0,"truncated trace record");std::vector<TraceRecord> out;
    for(std::size_t i=0;i<bytes.size();i+=29) {
        TraceRecord r{le(bytes,i,8),le(bytes,i+8,8),std::uint32_t(le(bytes,i+16,4)),bytes[i+20],le(bytes,i+21,8)};
        require(r.operation<=1,"invalid trace operation");out.push_back(r);
    }return out;
}
// RFC4180 fields including embedded commas/newlines and doubled quotes.
inline std::vector<std::vector<std::string>> read_csv(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::vector<std::string>> rows; std::vector<std::string> row;std::string field;
    bool quoted=false,closed=false;
    for(std::size_t i=0;i<bytes.size();++i) {
        char c=char(bytes[i]);
        if(quoted) {
            if(c=='"') {if(i+1<bytes.size()&&bytes[i+1]=='"'){field+='"';++i;}else{quoted=false;closed=true;}}
            else field+=c;
        } else if(c=='"') {require(field.empty()&&!closed,"invalid CSV quote");quoted=true;}
        else if(c==','||c=='\n') {row.push_back(field);field.clear();closed=false;if(c=='\n'){rows.push_back(row);row.clear();}}
        else {require(!closed,"invalid CSV after quote");field+=c;}
    }
    require(!quoted,"unterminated CSV quote");
    if(!field.empty()||!row.empty()||closed){row.push_back(field);rows.push_back(row);}return rows;
}
struct Resource {std::uint64_t id,bytes;std::string source_path,extension;std::vector<std::uint64_t> dependencies;};
inline std::vector<Resource> read_resources(const std::vector<std::uint8_t>& bytes) {
    auto rows=read_csv(bytes);
    require(!rows.empty()&&rows[0]==std::vector<std::string>{"id","source_path","bytes","extension","dependency_ids"},"invalid resource header");
    std::vector<Resource> out;
    auto number=[](const std::string& s){Json j;j.type=Json::Type::number;j.value=s;return j.u64();};
    for(std::size_t i=1;i<rows.size();++i) {
        const auto& r=rows[i];require(r.size()==5,"invalid resource row");
        Resource v{number(r[0]),number(r[2]),r[1],r[3],{}};
        require(v.id==i-1,"nonsequential resource id");
        std::istringstream deps(r[4]);std::string token;
        while(std::getline(deps,token,';')) {auto id=number(token);require(id<rows.size()-1,"invalid dependency id");v.dependencies.push_back(id);}
        out.push_back(std::move(v));
    }return out;
}
} // namespace ecl::workload
