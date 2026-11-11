#include "pe_image.hpp"
#include <fstream>
#include <functional>
#include <limits>
#include <algorithm>
namespace rnet {
namespace {
void bounds(const Bytes& b, std::size_t p, std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated PE/resource");}
std::uint16_t u16(const Bytes& b,std::size_t p){bounds(b,p,2);return std::uint16_t(b[p])|(std::uint16_t(b[p+1])<<8);}
std::uint32_t u32(const Bytes& b,std::size_t p){return u16(b,p)|(std::uint32_t(u16(b,p+2))<<16);}
std::u16string textz(const Bytes& b,std::size_t& p){std::u16string s;for(;;){auto c=u16(b,p);p+=2;if(!c)return s;s.push_back(char16_t(c));}}
}
Bytes read_file(const std::filesystem::path& p){
    std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open file: "+p.u8string());
    auto len=f.tellg();if(len<0||len>256*1024*1024)throw std::runtime_error("Invalid file size");
    Bytes b(static_cast<std::size_t>(len));f.seekg(0);if(!b.empty()&&!f.read(reinterpret_cast<char*>(b.data()),len))throw std::runtime_error("File read failed");return b;
}
std::uint16_t PeImage::word(std::size_t p)const{return u16(data_,p);}
std::uint32_t PeImage::dword(std::size_t p)const{return u32(data_,p);}
PeImage::PeImage(Bytes b):data_(std::move(b)){
    if(word(0)!=0x5a4d)throw std::runtime_error("Missing DOS header");
    auto pe=dword(60);if(dword(pe)!=0x4550||word(pe+4)!=0x14c)throw std::runtime_error("Expected i386 PE");
    optional_=pe+24;if(word(optional_)!=0x10b)throw std::runtime_error("Expected PE32");
    auto s=optional_+word(pe+20);auto n=word(pe+6);if(n>96)throw std::runtime_error("Too many PE sections");
    for(unsigned i=0;i<n;++i,s+=40){bounds(data_,s,40);sections_.push_back({dword(s+12),dword(s+8),dword(s+20),dword(s+16)});}
}
std::size_t PeImage::offset(std::uint32_t r)const{
    if(r<dword(optional_+60)){bounds(data_,r,1);return r;}
    for(const auto& s:sections_)if(r>=s.address&&std::uint64_t(r)<std::uint64_t(s.address)+std::max(s.raw_size,s.size)){
        auto delta=r-s.address;if(delta>=s.raw_size)throw std::runtime_error("RVA points into uninitialized data");
        std::size_t p=std::size_t(s.offset)+delta;bounds(data_,p,1);return p;
    }
    throw std::runtime_error("PE address outside image");
}
std::string PeImage::ascii(std::size_t p)const{std::string s;for(;;++p){bounds(data_,p,1);if(!data_[p])return s;s.push_back(char(data_[p]));}}
Bytes PeImage::read_va(std::uint32_t va,std::size_t size)const{
    const auto base=dword(optional_+28);if(va<base)throw std::runtime_error("VA before image base");
    auto p=offset(va-base);bounds(data_,p,size);return Bytes(data_.begin()+p,data_.begin()+p+size);
}
std::uint32_t PeImage::entry_point()const{return dword(optional_+28)+dword(optional_+16);}
std::vector<Resource> PeImage::resources()const{
    auto rva=dword(optional_+112);if(!rva)return {};
    auto root=offset(rva);std::vector<Resource> result;
    auto name=[&](std::uint32_t n){
        if(!(n&0x80000000))return std::to_string(n);
        auto p=root+(n&0x7fffffff);auto len=word(p);std::u16string s;
        for(unsigned i=0;i<len;++i)s.push_back(char16_t(word(p+2+i*2)));
        return utf8(s);
    };
    std::function<void(std::size_t,std::vector<std::string>)> walk;
    walk=[&](std::size_t p,std::vector<std::string> path){
        if(path.size()>2)throw std::runtime_error("Invalid resource depth");
        auto n=unsigned(word(p+12))+word(p+14);bounds(data_,p+16,std::size_t(n)*8);
        for(unsigned i=0;i<n;++i){auto e=p+16+i*8;auto id=dword(e);auto v=dword(e+4);auto next=path;next.push_back(name(id));
            if(v&0x80000000)walk(root+(v&0x7fffffff),next);
            else{
                if(next.size()!=3)throw std::runtime_error("Incomplete resource path");
                auto d=root+v;auto from=offset(dword(d));auto size=dword(d+4);bounds(data_,from,size);
                result.push_back({next[0],next[1],std::uint32_t(std::stoul(next[2])),Bytes(data_.begin()+from,data_.begin()+from+size)});
                if(result.size()>100000)throw std::runtime_error("Too many resources");
            }
        }
    };
    walk(root,{});return result;
}
std::vector<Import> PeImage::imports()const{
    auto rva=dword(optional_+104);if(!rva)return {};
    std::vector<Import> out;auto p=offset(rva);
    for(unsigned i=0;dword(p+12);++i,p+=20){
        if(i>=4096)throw std::runtime_error("Invalid import directory");
        Import entry;entry.dll=ascii(offset(dword(p+12)));auto t=offset(dword(p)?dword(p):dword(p+16));
        for(unsigned j=0;dword(t);++j,t+=4){if(j>=65536)throw std::runtime_error("Invalid thunk table");auto v=dword(t);entry.symbols.push_back(v&0x80000000?"#"+std::to_string(v&65535):ascii(offset(v)+2));}
        out.push_back(std::move(entry));
    }
    return out;
}
std::vector<TextResource> PeImage::strings()const{
    std::vector<TextResource> out;
    for(const auto& r:resources())if(r.type=="6"){
        std::size_t p=0;auto block=std::stoul(r.name);
        for(unsigned i=0;i<16;++i){auto len=u16(r.bytes,p);p+=2;std::u16string s;
            for(unsigned j=0;j<len;++j,p+=2)s.push_back(char16_t(u16(r.bytes,p)));
            if(!s.empty())out.push_back({std::uint32_t((block-1)*16+i),std::move(s)});
        }
    }
    return out;
}
std::vector<MenuItem> PeImage::menu(const Bytes& b){
    if(u16(b,0)!=0)throw std::runtime_error("Unsupported extended menu template");
    std::size_t p=4+u16(b,2);
    std::function<std::vector<MenuItem>(unsigned)> parse;
    parse=[&](unsigned depth){
        if(depth>32)throw std::runtime_error("Menu nesting too deep");
        std::vector<MenuItem> out;
        for(;;){MenuItem e;e.flags=u16(b,p);p+=2;if(!(e.flags&16)){e.id=u16(b,p);p+=2;}
            e.text=textz(b,p);if(e.flags&16)e.children=parse(depth+1);auto end=e.flags&128;out.push_back(std::move(e));if(end)return out;
        }
    };
    return parse(0);
}
std::string utf8(const std::u16string& s){
    std::string out;
    for(std::size_t i=0;i<s.size();++i){std::uint32_t c=s[i];
        if(c>=0xd800&&c<=0xdbff){if(i+1==s.size()||s[i+1]<0xdc00||s[i+1]>0xdfff)throw std::runtime_error("Invalid UTF-16");c=0x10000+((c-0xd800)<<10)+(s[++i]-0xdc00);}
        else if(c>=0xdc00&&c<=0xdfff)throw std::runtime_error("Invalid UTF-16");
        if(c<0x80)out.push_back(char(c));
        else if(c<0x800){out.push_back(char(0xc0|(c>>6)));out.push_back(char(0x80|(c&63)));}
        else if(c<0x10000){out.push_back(char(0xe0|(c>>12)));out.push_back(char(0x80|((c>>6)&63)));out.push_back(char(0x80|(c&63)));}
        else{out.push_back(char(0xf0|(c>>18)));out.push_back(char(0x80|((c>>12)&63)));out.push_back(char(0x80|((c>>6)&63)));out.push_back(char(0x80|(c&63)));}
    }return out;
}
}
