#include "database.hpp"
#include "runtime_resources.hpp"
#include <iostream>
#include <algorithm>
std::string reference_text(rnet::ArchiveReader& a){auto n=a.u32();if(n>8*1024*1024)throw std::runtime_error("Reference string too large");std::string s;for(std::uint32_t i=0;i<n;++i)s.push_back(char(a.u8()));return s;}
int main(int argc,char**argv){try{
    if(argc!=5&&argc!=6)throw std::runtime_error("Original EXE, RND and independent parameter/storage references required");
    auto bytes=rnet::bundled_catalog_bytes();
    auto catalog=rnet::ParameterCatalog::read(bytes);
    std::cout<<"Parameters "<<catalog.parameters.size()<<" start "<<catalog.parameter_offset<<" end "<<catalog.parameter_end<<'\n';
    auto reference=rnet::read_file(argv[3]);rnet::ArchiveReader r(reference);if(r.u32()!=0x32504e52)throw std::runtime_error("Unsupported reference version");auto count=r.u32();
    if(catalog.parameters.size()!=2154||count!=2154)throw std::runtime_error("Unexpected parameter count");
    unsigned mismatches=0;
    for(std::uint32_t i=0;i<count;++i){auto id=r.u32(),compound=r.u32();auto name=reference_text(r),help=reference_text(r),group=reference_text(r);std::array<std::uint32_t,13> metadata;for(auto&v:metadata)v=r.u32();bool sign=r.boolean(),flag70=r.boolean();
        auto it=catalog.parameters.find(id);if(it==catalog.parameters.end())throw std::runtime_error("Missing parameter ID");auto&p=it->second;
        if(p.id!=compound||group!=rnet::utf8(p.text88)||flag70!=p.flag70||name!=rnet::utf8(p.name())||help!=rnet::utf8(p.help())||metadata!=p.metadata||sign!=p.signed_storage){
            if(mismatches<8)std::cerr<<"Mismatch ID "<<id<<" name="<<(name==rnet::utf8(p.name()))<<" help="<<(help==rnet::utf8(p.help()))<<" metadata="<<(metadata==p.metadata)<<" signed="<<(sign==p.signed_storage)<<'\n';
            for(unsigned k=0;k<13;++k)if(metadata[k]!=p.metadata[k])std::cerr<<"  metadata["<<k<<"] original="<<metadata[k]<<" serialized="<<p.metadata[k]<<" parameter="<<rnet::utf8(p.name())<<"\n";
            ++mismatches;
        }
    }
    if(r.position()!=reference.size())throw std::runtime_error("Trailing reference bytes");
    if(mismatches)throw std::runtime_error(std::to_string(mismatches)+" parameter definitions differ from original runtime");
    bool rejected=false;auto truncated=bytes;truncated.resize(catalog.parameter_end-1);
    try{rnet::ParameterCatalog::read(truncated);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Truncated parameter table accepted");
    auto storage=rnet::read_file(argv[4]);rnet::ArchiveReader sr(storage);
    if(sr.u32()!=0x31534e52)throw std::runtime_error("Unsupported storage reference");
    const auto map_count=sr.u32();if(map_count!=29||catalog.storage_maps.size()!=map_count)throw std::runtime_error("Storage map count differs");
    for(unsigned i=0;i<map_count;++i){
        auto id=sr.u32(),version=sr.u32();auto name=reference_text(sr);
        auto key=std::uint64_t(id)|(std::uint64_t(version)<<32);auto it=catalog.storage_maps.find(key);
        if(it==catalog.storage_maps.end()||rnet::utf8(it->second.name)!=name)throw std::runtime_error("Storage map identity or name differs");
        for(auto& group:it->second.groups){
            if(sr.u32()!=group.instances||sr.u32()!=group.stride||sr.u32()!=group.last_section)throw std::runtime_error("Storage group differs");
            for(auto offset:group.offsets)if(sr.u32()!=offset)throw std::runtime_error("Storage section offset differs");
            for(auto& section:group.sections){
                if(sr.u32()!=section.items.size())throw std::runtime_error("Storage item count differs");
                for(auto& item:section.items)if(sr.u32()!=item.id||sr.u32()!=item.width)throw std::runtime_error("Storage item differs");
                for(auto field:section.fields)if(sr.u32()!=field)throw std::runtime_error("Storage section metadata differs");
            }
        }
    }
    if(sr.position()!=storage.size())throw std::runtime_error("Trailing storage reference bytes");
    if(argc==6){std::map<std::u16string,unsigned> groups;for(auto& entry:catalog.parameters)++groups[entry.second.text88];for(auto& g:groups)std::cout<<g.second<<" | "<<rnet::utf8(g.first)<<'\n';}
    std::cout<<"PASS: all 29 storage maps, groups, sections, offsets and item widths match isolated original process\n";
    std::cout<<"PASS: all 2154 keys, compound IDs, names, help texts, groups, 13 metadata fields and storage/access flags match isolated original process; truncation rejected\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
