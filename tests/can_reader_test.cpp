#include "can_reader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace rnet;
struct Entry {bool tx;CanFrame frame;};
struct RecordedTransport:CanTransport {
    std::vector<Entry> entries;std::size_t position=0;
    std::atomic<bool>* cancel_after_acquire=nullptr;
    void send(const CanFrame& frame)override{
        if(position==entries.size()||!entries[position].tx||entries[position].frame.id!=frame.id||entries[position].frame.data!=frame.data)
            throw std::runtime_error("Client request differs from independently recorded original CAN request at "+std::to_string(position));
        ++position;
    }
    bool receive(CanFrame& frame,unsigned)override{
        if(position==entries.size()||entries[position].tx)throw std::runtime_error("Client waited instead of sending recorded request");
        frame=entries[position++].frame;if(cancel_after_acquire&&position==2)*cancel_after_acquire=true;return true;
    }
};
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Fixture directory required");
    auto fixtures=std::filesystem::path(argv[1]);auto plan=read_can_plan(fixtures/"emulator-read-plan.txt");
    RecordedTransport recorded;std::ifstream input(fixtures/"emulator-pop-read-trace.txt");std::string line;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);std::string direction,identifier,data;row>>direction>>identifier>>data;
        if(!row||data.size()%2&&data!="-")throw std::runtime_error("Malformed trace fixture");
        Entry entry{direction=="TX",{std::uint32_t(std::stoul(identifier,nullptr,16)),{}}};
        if(data!="-")for(std::size_t i=0;i<data.size();i+=2)entry.frame.data.push_back(std::uint8_t(std::stoul(data.substr(i,2),nullptr,16)));
        recorded.entries.push_back(std::move(entry));
    }
    std::atomic<bool> cancelled{false};CanRepositoryReader reader(recorded,cancelled);std::vector<RepositoryFile> files;
    for(auto selector:plan)files.push_back(reader.read(selector));
    if(recorded.position!=recorded.entries.size()||files.size()!=20)throw std::runtime_error("Incomplete original read trace");
    // Controller traffic can share the same CAN ID/object while being a
    // request to client0, a reply to E, or a reply from another phase.
    RecordedTransport noisy;
    for(const auto &entry:recorded.entries){
        if(!entry.tx&&entry.frame.id==0x790&&entry.frame.data.size()==8){
            for(auto code:{0x40u,0x2eu,0xceu,entry.frame.data[0]==0x4f?0x2fu:0x4fu}){
                auto unrelated=entry;unrelated.frame.data[0]=code;noisy.entries.push_back(unrelated);
            }
        }
        noisy.entries.push_back(entry);
    }
    CanRepositoryReader withTraffic(noisy,cancelled);
    for(std::size_t i=0;i<plan.size();++i)if(withTraffic.read(plan[i]).data!=files[i].data)
        throw std::runtime_error("Unrelated same-object traffic corrupted read");
    if(noisy.position!=noisy.entries.size())throw std::runtime_error("Unrelated traffic was not consumed correctly");
    auto original=RnetDocument::read(read_file(fixtures/"emulator-original-read.R-net"));unsigned matched=0;
    for(const auto& saved:original.files)for(const auto& received:files)
        if(saved.id==received.id&&saved.version==received.version&&saved.data==received.data)++matched;
    if(matched!=16)throw std::runtime_error("Independent original-saved payload comparison differs");
    for(unsigned node:{1u,2u,3u}){
        RecordedTransport routed;routed.entries=recorded.entries;
        for(auto& e:routed.entries){
            if(e.tx&&e.frame.id==0x78f)e.frame.data[0]|=node;
            else if(!e.tx&&e.frame.id==0x790)e.frame.id+=node;
            else if(!e.tx&&(e.frame.id&0xfffe0000)==0x1e420000)e.frame.id+=node<<18;
        }
        CanRepositoryReader remote(routed,cancelled,node);
        for(std::size_t i=0;i<plan.size();++i)if(remote.read(plan[i]).data!=files[i].data)throw std::runtime_error("Routed node payload differs");
        if(routed.position!=routed.entries.size())throw std::runtime_error("Routed read trace incomplete");
    }
    {
        RecordedTransport absent;
        absent.entries={{true,{0x78f,{0x20,0x80,0,0,0x1f,0,0,0}}},{false,{0x790,{0x4f,0x80,0,0,0,0,0,0}}},
            {true,{0x78f,{0x20,0x81,0,0,3,0,0,0}}},{false,{0x790,{0xcf,0x81,0,0,0x25,0,0,0}}},
            {true,{0x78f,{0x20,0x80,0,0,0,0,0,0}}},{false,{0x790,{0x4f,0x80,0,0,0,0,0,0}}}};
        bool denied=false;try{CanRepositoryReader r(absent,cancelled);r.read(3);}catch(const PopRejected& e){denied=e.odi==0x81&&e.code==0x25;}
        if(!denied||absent.position!=absent.entries.size())throw std::runtime_error("Absent file did not release ownership cleanly");
        RecordedTransport stopped;
        stopped.entries={absent.entries[0],absent.entries[1],absent.entries[4],absent.entries[5]};
        stopped.cancel_after_acquire=&cancelled;
        bool rejected=false;try{CanRepositoryReader r(stopped,cancelled);r.read(3);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||stopped.position!=stopped.entries.size())throw std::runtime_error("Cancellation did not release acquired repository");
        cancelled=false;
    }
    for(unsigned damage=0;damage<4;++damage){
        RecordedTransport broken;broken.entries=recorded.entries;
        for(auto& entry:broken.entries){
            if(damage<3&&(entry.frame.id&0xfffe0000)==0x1e420000){
                if(damage==0)entry.frame.id+=1;
                if(damage==1)entry.frame.data[0]^=1;
                if(damage==2)entry.frame.data.resize(9);
                break;
            }
            if(damage==3&&!entry.tx&&entry.frame.id==0x790&&entry.frame.data[0]==0x2f&&entry.frame.data[1]==0x8b){entry.frame.data[4]^=1;break;}
        }
        bool rejected=false;try{CanRepositoryReader invalid(broken,cancelled);invalid.read(plan.front());}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Corrupt transfer accepted");
    }
    RecordedTransport stopped;cancelled=true;bool rejected=false;
    try{CanRepositoryReader invalid(stopped,cancelled);invalid.read(plan.front());}catch(const std::runtime_error&){rejected=true;}
    if(!rejected||stopped.position)throw std::runtime_error("Cancelled reader sent a request");
    std::cout<<"PASS: 20 POP reads match 990 independently recorded CAN frames; 16 original-saved payload matches; sequence, size, transfer/file CRC and cancellation rejection\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
