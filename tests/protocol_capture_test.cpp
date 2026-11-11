#include "protocol_capture.hpp"
#include "values.hpp"
#include <iostream>
#include <set>
#include <functional>
struct Frame {std::uint32_t packet=0,id=0;rnet::Bytes data;};
std::vector<Frame> frames(const std::filesystem::path& path){
    auto bytes=rnet::read_file(path);rnet::ArchiveReader reader(bytes);
    if(reader.u32()!=0x3143524f)throw std::runtime_error("Unsupported capture fixture");
    const auto count=reader.u32();if(count>1000000)throw std::runtime_error("Capture fixture too large");
    std::vector<Frame> result;std::uint32_t previous=0;
    for(unsigned i=0;i<count;++i){Frame frame;frame.packet=reader.u32();frame.id=reader.u32();const auto length=reader.u8();if(length>8||frame.packet<=previous)throw std::runtime_error("Invalid capture fixture frame");previous=frame.packet;
        for(unsigned j=0;j<8;++j){auto byte=reader.u8();if(j<length)frame.data.push_back(byte);else if(byte)throw std::runtime_error("Nonzero capture fixture padding");}result.push_back(std::move(frame));}
    if(reader.position()!=bytes.size())throw std::runtime_error("Trailing capture fixture bytes");return result;
}
void rejects(const std::function<void()>& operation){bool rejected=false;try{operation();}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("Malformed transfer was accepted");}
void check_payload(const rnet::Bytes& bytes){
    rnet::RnetDocument document;rnet::RepositoryFile file;file.id=0x7ffffffe;file.version=1;file.data=bytes;document.files.push_back(std::move(file));
    rnet::ParameterCatalog unknown_catalog;std::vector<std::string> warnings;
    if(!rnet::read_values(document,unknown_catalog,&warnings).empty()||warnings.size()!=1)throw std::runtime_error("Opaque captured repository payload was not reported");
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Private open-rnet fixture directory required");const std::filesystem::path root=argv[1];
    std::set<rnet::Bytes> document_payloads;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root/"documents"))if(entry.is_regular_file()&&entry.path().extension()==".R-net")for(auto& file:rnet::RnetDocument::read(rnet::read_file(entry.path())).files)document_payloads.insert(file.data);
    const std::vector<std::vector<unsigned>> sizes{{13,56,90,464,218,30,555,59,35,457,55,168,95,38,57,450},{13,841,55,55,555,30,218,59,90,56,464,35,457,457,168,95,38,57,450}};
    const unsigned expected_matches[]={14,17};unsigned capture=0,total=0,matched_total=0;
    for(const auto* name:{"programmer_dump_file_july2017.bin","programmer_write_file_july2017.bin"}){
        auto source=frames(root/name);rnet::CapturedBlockAssembler assembler;std::vector<rnet::Bytes> blocks;std::vector<Frame> first;
        for(const auto& frame:source){
            if((frame.id&0xfffe0000)==0x1e460000&&first.size()<2)first.push_back(frame);
            if(auto complete=assembler.push(frame.id,frame.data))blocks.push_back(std::move(*complete));
        }
        assembler.finish();if(blocks.size()!=sizes[capture].size())throw std::runtime_error("Observed transfer count differs");
        unsigned matched=0;
        for(std::size_t i=0;i<blocks.size();++i){if(blocks[i].size()!=sizes[capture][i])throw std::runtime_error("Observed transfer boundary differs");check_payload(blocks[i]);if(document_payloads.count(blocks[i]))++matched;}
        if(matched!=expected_matches[capture])throw std::runtime_error("Captured blocks no longer match exported repository files");
        if(first.size()!=2)throw std::runtime_error("Missing first transfer fixture");
        rnet::CapturedBlockAssembler truncated;truncated.push(first[0].id,first[0].data);rejects([&]{truncated.finish();});
        if(truncated.push(first[1].id,first[1].data)!=std::optional<rnet::Bytes>(blocks.front()))throw std::runtime_error("Resumed transfer differs");
        rnet::CapturedBlockAssembler order;rejects([&]{order.push(first[1].id,first[1].data);});order.push(first[0].id,first[0].data);rejects([&]{order.push(first[0].id,first[0].data);});
        rnet::CapturedBlockAssembler limited(4);rejects([&]{limited.push(first[0].id,first[0].data);});
        rejects([&]{assembler.push(0x20000000,{});});rejects([&]{assembler.push(first[0].id,rnet::Bytes(9,0));});
        auto corrupt=blocks.front();corrupt.back()^=1;rejects([&]{check_payload(corrupt);});
        std::cout<<name<<": "<<blocks.size()<<" complete repository payloads, "<<matched<<" exact exported-file matches\n";
        total+=blocks.size();matched_total+=matched;++capture;
    }
    std::cout<<"PASS: "<<total<<" offline captured transfers; "<<matched_total<<" match actual document payloads; invalid/missing/duplicate segments rejected; no transport or device used\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
