#include "can_socket.hpp"
#include "database.hpp"
#include "runtime_resources.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <csignal>
namespace {
std::atomic<bool> cancelled{false};
void cancel(int){cancelled=true;}
void save(const std::filesystem::path& path,const rnet::Bytes& bytes){
    if(std::filesystem::exists(path))throw std::runtime_error("Refuse existing snapshot output");
    std::ofstream output(path,std::ios::binary);if(!output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()))throw std::runtime_error("Snapshot write failed");
}
}
int main(int argc,char** argv){try{
    if(argc!=4&&argc!=6)throw std::runtime_error("Usage: rnet_config_read INTERFACE OUTPUTDIR inspect|read");
    std::string mode=argv[argc-1];if(mode!="inspect"&&mode!="read")throw std::runtime_error("Choose inspect or read");
    std::filesystem::path output=argv[argc==6?3:2];
    if(std::filesystem::exists(output))throw std::runtime_error("Choose a new snapshot directory");
    std::filesystem::create_directories(output);
    std::signal(SIGINT,cancel);std::signal(SIGTERM,cancel);
    auto catalog=rnet::bundled_catalog();
    auto reference=rnet::RnetDocument::read(rnet::bundled_default_document());
    std::set<std::uint32_t> candidates;
    for(const auto& entry:catalog.file_names)candidates.insert(entry.first);
    for(const auto& entry:catalog.storage_maps)candidates.insert(entry.second.id);
    for(const auto& entry:reference.files)candidates.insert(entry.id);
    // Additional selectors observed in the original's successful complete read,
    // including the currently identified 0102/5003 device family.
    candidates.insert(0x1060000);candidates.insert(0x10017);candidates.insert(0x1025003);
    std::ofstream manifest(output/"manifest.csv");manifest<<"node,selector,version,bytes,status,code\n";
    auto transport=rnet::open_can_read_transport(argv[1],output/"can.log");
    unsigned total=0,errors=0;
    for(unsigned node=0;node<4&&!cancelled;++node){
        rnet::CanRepositoryReader reader(*transport,cancelled,node);
        try{auto status=reader.repository_status();std::cout<<"REPOSITORY node="<<node<<" owner_status="<<status<<std::endl;}
        catch(const rnet::PopRejected& e){std::cout<<"NO_REPOSITORY node="<<node<<" code="<<e.code<<std::endl;manifest<<node<<",0,0,0,no_repository,"<<e.code<<'\n';continue;}
        if(mode=="inspect")continue;
        auto document=reference;document.files.clear();document.comments.push_back(u"CAN read backup; encrypted metadata retained from default template");
        auto directory=output/("node-"+std::to_string(node));std::filesystem::create_directory(directory);
        for(auto selector:candidates){
            if(cancelled)break;
            try{
                auto file=reader.read(selector);auto name=std::to_string(file.id)+"-v"+std::to_string(file.version)+".bin";
                save(directory/name,file.data);
                manifest<<node<<','<<selector<<','<<file.version<<','<<file.data.size()<<",read,0\n";manifest.flush();
                std::cout<<"READ node="<<node<<" selector="<<std::hex<<selector<<std::dec<<" version="<<file.version<<" bytes="<<file.data.size()<<std::endl;
                document.files.push_back(std::move(file));++total;
            }catch(const rnet::PopRejected& e){
                if(e.odi!=0x81||e.code!=0x25)throw;
                manifest<<node<<','<<selector<<",0,0,absent,"<<e.code<<'\n';
            }
        }
        if(!document.files.empty()){
            auto bytes=document.write();if(rnet::RnetDocument::read(bytes).write()!=bytes)throw std::runtime_error("Snapshot roundtrip differs");
            save(output/("node-"+std::to_string(node)+"-configuration.R-net"),bytes);
        }
        auto owner=reader.repository_status();std::cout<<"RELEASE_VERIFIED node="<<node<<" owner_status="<<owner<<std::endl;
        if(owner!=0)throw std::runtime_error("Repository ownership was not released");
    }
    std::cout<<"COMPLETE mode="<<mode<<" files="<<total<<" errors="<<errors<<" cancelled="<<cancelled.load()<<" candidate_selectors="<<candidates.size()<<std::endl;
    if(mode=="read"&&!total&&!cancelled)throw std::runtime_error("No configuration files received");
    return cancelled?2:0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
