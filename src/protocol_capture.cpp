#include "protocol_capture.hpp"
namespace rnet {
void CapturedBlockAssembler::reject(const char* message){
    payload_.clear();expected_=1;active_=false;throw std::runtime_error(message);
}
std::optional<Bytes> CapturedBlockAssembler::push(std::uint32_t id,const Bytes& data){
    if(id>0x1fffffff||data.size()>8)reject("Invalid classic CAN frame");
    if((id&0xfffe0000)!=0x1e460000)return std::nullopt;
    const auto sequence=id&0xffff;
    if(sequence!=expected_||(!active_&&sequence!=1))reject("Missing, duplicate or reordered capture segment");
    if(data.empty())reject("Empty capture data segment");
    if(payload_.size()>limit_||data.size()>limit_-payload_.size())reject("Capture block exceeds size limit");
    active_=true;payload_.insert(payload_.end(),data.begin(),data.end());++expected_;
    if(id&0x10000){Bytes result;result.swap(payload_);expected_=1;active_=false;return result;}
    return std::nullopt;
}
void CapturedBlockAssembler::finish()const{
    if(active_)throw std::runtime_error("Truncated capture transfer");
}
}
