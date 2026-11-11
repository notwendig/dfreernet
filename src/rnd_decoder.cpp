#include "rnd_decoder.hpp"
#include <algorithm>
#ifdef __linux__
#include "runtime_resources.hpp"
#endif
namespace rnet {
namespace {
std::uint32_t word(const Bytes& b,std::size_t p){return std::uint32_t(b.at(p))|(std::uint32_t(b.at(p+1))<<8)|(std::uint32_t(b.at(p+2))<<16)|(std::uint32_t(b.at(p+3))<<24);}
void put(Bytes& b,std::size_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(v>>(i*8));}
}
std::array<std::uint8_t,31> RndDecoder::file_key(){
    // 005bfee0 -> 005bfd40 -> 00592a20 in the original image.
    std::array<std::uint32_t,351> state{};std::uint32_t x=0x672d83e1;
    for(auto& v:state){x=x*0x1c8e815U-1;v=x;}
    for(unsigned i=0;i<351;++i){auto y=(state[i]&0xfff80000U)|(state[(i+1)%351]&0x0007ffffU);state[i]=state[(i+175)%351]^(y>>1)^((y&1)?0xe4bd75f5U:0U);}
    std::array<std::uint8_t,31> key{};
    for(unsigned i=0;i<key.size();++i){x=state[i];x^=x>>11;x^=(x&0x00cabca5U)<<7;x^=(x&0xffffffabU)<<15;x^=x>>17;
        key[i]=std::uint8_t(std::min<std::uint64_t>(254,1+((std::uint64_t(x)*254)>>32)));
    }
    return key;
}
std::uint32_t RndDecoder::f(std::uint32_t x)const{return ((s_[x>>24]+s_[256+((x>>16)&255)])^s_[512+((x>>8)&255)])+s_[768+(x&255)];}
void RndDecoder::enc(std::uint32_t& l,std::uint32_t& r)const{
    for(unsigned i=0;i<16;++i){l^=p_[i];r^=f(l);std::swap(l,r);}std::swap(l,r);r^=p_[16];l^=p_[17];
}
void RndDecoder::dec(std::uint32_t& l,std::uint32_t& r)const{
    for(unsigned i=17;i>1;--i){l^=p_[i];r^=f(l);std::swap(l,r);}std::swap(l,r);r^=p_[1];l^=p_[0];
}
#ifdef __linux__
RndDecoder::RndDecoder(){
    const auto seed=assets::seed();
    for(unsigned i=0;i<p_.size();++i)p_[i]=word(seed,i*4);
    for(unsigned i=0;i<s_.size();++i)s_[i]=word(seed,72+i*4);
    initialize();
}
#endif
RndDecoder::RndDecoder(const PeImage& original){
    if(original.entry_point()!=0x42b160)throw std::runtime_error("Unsupported original EXE version for RND constants");
    const auto seed=original.read_va(0x6c2e70,18*4+1024*4);
    for(unsigned i=0;i<p_.size();++i)p_[i]=word(seed,i*4);
    for(unsigned i=0;i<s_.size();++i)s_[i]=word(seed,72+i*4);
    if(p_[0]!=0x243f6a88||s_[0]!=0xd1310ba6)throw std::runtime_error("Unexpected Blowfish constants in original EXE");
    initialize();
}
void RndDecoder::initialize(){
    const auto key=file_key();unsigned j=0;
    for(auto& v:p_){std::uint32_t a=0;for(unsigned k=0;k<4;++k){a=(a<<8)|key[j];j=(j+1)%key.size();}v^=a;}
    std::uint32_t l=0,r=0;
    for(unsigned i=0;i<p_.size();i+=2){enc(l,r);p_[i]=l;p_[i+1]=r;}
    for(unsigned i=0;i<s_.size();i+=2){enc(l,r);s_[i]=l;s_[i+1]=r;}
}
Bytes RndDecoder::decode(const Bytes& input)const{
    if(input.empty()||input.size()%8)throw std::runtime_error("RND ciphertext must consist of complete 8-byte blocks");
    Bytes out=input;
    for(std::size_t i=0;i<out.size();i+=8){auto l=word(out,i),r=word(out,i+4);dec(l,r);put(out,i,l);put(out,i+4,r);}
    auto version=word(out,0);if(version<31||version>33)throw std::runtime_error("RND decryption failed or database schema unsupported");
    return out;
}
}
