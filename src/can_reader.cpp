#include "can_reader.hpp"
#include <chrono>
#include <fstream>
#include <set>
namespace rnet {
namespace {
std::uint32_t u32(const Bytes& bytes,std::size_t offset){
    if(offset+4>bytes.size())throw std::runtime_error("Short POP response");
    std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset+i])<<(8*i);return value;
}
void append32(Bytes& bytes,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(std::uint8_t(value>>(8*i)));}
}
bool repository_reply(const CanFrame &frame,unsigned node,std::uint8_t odi,std::initializer_list<std::uint8_t> codes){
    if(frame.id!=0x790+node || frame.data.size()!=8 || frame.data[1]!=odi || frame.data[2] || frame.data[3])return false;
    for(auto code:codes)if(frame.data[0]==code)return true;
    return false;
}
CanFrame CanRepositoryReader::wait(const std::function<bool(const CanFrame&)>& matches,unsigned timeout_ms){
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
    while(std::chrono::steady_clock::now()<end){
        if(cancelled_)throw std::runtime_error("CAN read cancelled");
        CanFrame frame;if(transport_.receive(frame,100)&&matches(frame))return frame;
    }
    throw CanResponseTimeout();
}
void CanRepositoryReader::send(std::uint8_t tc,std::uint8_t odi,std::uint32_t value){
    if(cancelled_)throw std::runtime_error("CAN read cancelled");
    Bytes bytes{std::uint8_t(tc|node_),odi,0,0};append32(bytes,value);transport_.send({0x78f,std::move(bytes)});
}
std::uint32_t CanRepositoryReader::quick(std::uint8_t tc,std::uint8_t odi,std::uint32_t value){
    send(tc,odi,value);
    auto frame=wait([&](const auto& f){return repository_reply(f,node_,odi,{std::uint8_t(tc==0x20?0x4f:0x2f),0xcf});});
    if(frame.data[0]==0xcf)throw PopRejected(odi,u32(frame.data,4));
    if(frame.data[0]!=(tc==0x20?0x4f:0x2f))throw std::runtime_error("Unexpected POP transfer code");
    return u32(frame.data,4);
}
void CanRepositoryReader::detect(){
    // Require repeated, independent cyclic families; a UDP hello is not a
    // controller connection. This sends no wake/power or CAN test command.
    unsigned heartbeat=0,periodic=0;
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<end){
        auto f=wait([](const auto& x){return x.id==0x140c0001||x.id==0x02000300;},1500);
        if(f.data!=Bytes{0,0})continue;
        if(f.id==0x140c0001)++heartbeat;else ++periodic;
        if(heartbeat>=2&&periodic>=2)return;
    }
    throw std::runtime_error("No repeated emulator cyclic traffic");
}
RepositoryFile CanRepositoryReader::read(std::uint32_t selector){return read_impl(selector,false);}
RepositoryFile CanRepositoryReader::read_impl(std::uint32_t selector,bool retainOwnership){
    if(cancelled_)throw std::runtime_error("CAN read cancelled");
    bool acquired=false,transfer=false;
    try {
        acquired=true;
        try{quick(0x20,0x80,0x1f);}catch(const PopRejected&){acquired=false;throw;}
        quick(0x20,0x81,selector);
        RepositoryFile file;file.id=selector;file.version=quick(0x40,0x89);
        quick(0x20,0x84);
        auto size=quick(0x40,0x86);
        if(size<4||size>65535*8)throw std::runtime_error("Repository block size outside supported bounds");
        quick(0x20,0x85);
        // Observed request: low 24 bits length, high byte window=64.
        send(0x50,0x8c,size|0x40000000);
        transfer=true;
        auto start=wait([&](const auto& f){return repository_reply(f,node_,0x8c,{0x0f,0xcf});});
        if(start.data[0]==0xcf)throw PopRejected(0x8c,u32(start.data,4));
        if(start.data[0]!=0x0f||u32(start.data,4)!=size)throw std::runtime_error("POP transfer start size differs");
        std::uint32_t sequence=1;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        for(;;){
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("POP block transfer exceeded deadline");
            auto family=((0x790+node_)<<18)|0x20000;
            auto frame=wait([&](const auto& f){return (f.id&0xfffe0000)==family;});
            if((frame.id&65535)!=sequence||frame.data.empty()||frame.data.size()>8||file.data.size()+frame.data.size()>size)
                throw std::runtime_error("Missing, duplicated, oversized or unordered CAN segment");
            file.data.insert(file.data.end(),frame.data.begin(),frame.data.end());
            bool last=(frame.id&0x10000)!=0;
            if(!last&&frame.data.size()!=8)throw std::runtime_error("Short non-final CAN segment");
            if(last||sequence%64==0)transport_.send({0x1e3f0000|sequence,{}});
            ++sequence;
            if(last)break;
        }
        if(file.data.size()!=size)throw std::runtime_error("Incomplete repository payload");
        auto finish=wait([&](const auto& f){return repository_reply(f,node_,0x8c,{0x8f,0xcf});});
        if(finish.data[0]==0xcf)throw PopRejected(0x8c,u32(finish.data,4));
        if(finish.data[0]!=0x8f||u32(finish.data,4)!=RnetDocument::crc16(file.data.data(),file.data.size()))throw std::runtime_error("POP transfer CRC differs");
        send(0x80,0x8c);
        transfer=false;
        quick(0x40,0x8a);
        auto checksum=quick(0x40,0x8b);
        Bytes checked;append32(checked,selector);append32(checked,file.version);append32(checked,size);
        checked.insert(checked.end(),file.data.begin(),file.data.end());
        if(checksum!=RnetDocument::crc16(checked.data(),checked.size()))throw std::runtime_error("Repository identity/version/size CRC differs");
        RepositoryLayout::read(file.data);
        if(!retainOwnership) quick(0x20,0x80);
        acquired=false;
        return file;
    } catch(...) {
        // Release the read session on failure. Never send write/download or
        // power commands. No partial file is returned to the application.
        if(acquired)cleanup(transfer);
        throw;
    }
}
void CanRepositoryReader::cleanup(bool transfer)noexcept{
    try{
        // Release even after user cancellation; this is protocol cleanup, not
        // a parameter write. Never leave our ownership deliberately latched.
        if(transfer)transport_.send({0x78f,Bytes{std::uint8_t(0x80|node_),0x8c,0,0,0,0,0,0}});
        transport_.send({0x78f,Bytes{std::uint8_t(0x20|node_),0x80,0,0,0,0,0,0}});
        auto end=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        while(std::chrono::steady_clock::now()<end){CanFrame f;if(transport_.receive(f,100)&&f.id==0x790+node_&&f.data.size()==8&&f.data[0]==0x4f&&f.data[1]==0x80)return;}
    }catch(...){}
}
std::vector<std::uint32_t> read_can_plan(const std::filesystem::path& path){
    std::ifstream input(path);if(!input)throw std::runtime_error("Cannot read selector plan");
    std::vector<std::uint32_t> result;std::set<std::uint32_t> seen;std::string line;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line.size()>8||line.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("Expected one hexadecimal selector per line");
        auto selector=std::uint32_t(std::stoul(line,nullptr,16));
        if(!seen.insert(selector).second||result.size()>=256)throw std::runtime_error("Duplicate or excessive selectors");
        result.push_back(selector);
    }
    if(result.empty())throw std::runtime_error("Empty selector plan");
    return result;
}
}
