#include "document.hpp"
#include <set>
namespace rnet {
namespace {
class Writer {
public:
    Bytes bytes;
    void u8(std::uint8_t v){bytes.push_back(v);}
    void u16(std::uint16_t v){u8(v&255);u8(v>>8);}
    void u32(std::uint32_t v){u16(v&65535);u16(v>>16);}
    void count(std::size_t n){if(n>10000000)throw std::runtime_error("Document count exceeds limit");if(n<65535)u16(std::uint16_t(n));else{u16(65535);u32(std::uint32_t(n));}}
    void block(const Bytes& b){count(b.size());bytes.insert(bytes.end(),b.begin(),b.end());}
    void string_length(std::size_t n){if(n<255)u8(std::uint8_t(n));else if(n<65534){u8(255);u16(std::uint16_t(n));}else{u8(255);u16(65535);u32(std::uint32_t(n));}}
    void string(const std::u16string& s){
        // Preserve observed ANSI text for ASCII. Unicode form is canonical MFC.
        bool ascii=true;for(auto c:s)if(c>127)ascii=false;
        if(!ascii){u8(255);u16(65534);}string_length(s.size());
        for(auto c:s){if(ascii)u8(std::uint8_t(c));else u16(c);}
    }
};
Bytes block(ArchiveReader& a){auto n=a.count();Bytes b;b.reserve(n);for(std::uint32_t i=0;i<n;++i)b.push_back(a.u8());return b;}
}
RepositoryLayout RepositoryLayout::read(const Bytes& data){
    // Original 005c0370 / 005dc4a0: two byte-sized section counts,
    // followed by (instances,stride) pairs. 005e1f10 complements the byte sum.
    ArchiveReader a(data);RepositoryLayout result;result.checksum=a.u16();
    const std::array<unsigned,2> counts{a.u8(),a.u8()};
    result.header_size=4+2*(counts[0]+counts[1]);
    if(result.header_size>data.size())throw std::runtime_error("Truncated repository layout header");
    std::uint32_t sum=0;for(std::size_t i=2;i<result.header_size;++i)sum+=data[i];
    if(std::uint16_t(~sum)!=result.checksum)throw std::runtime_error("Invalid repository layout checksum or legacy payload");
    std::size_t position=result.header_size;
    // Original DAT_00665ba8 contains {1,8}, confirmed in the PE image.
    constexpr unsigned instances[2]={1,8};
    for(unsigned group=0;group<2;++group){
        for(unsigned i=0;i<counts[group];++i){
            RepositorySection section;section.instances=a.u8();section.stride=a.u8();
            section.offset=position+result.stride[group];
            result.stride[group]+=section.size();result.sections[group].push_back(section);
        }
        position+=result.stride[group]*instances[group];
    }
    if(position!=data.size())throw std::runtime_error("Repository layout size does not match payload");
    return result;
}
std::size_t RepositoryLayout::offset(unsigned group,unsigned instance,unsigned section)const{
    if(group>=2||instance>=(group==0?1u:8u)||section>=sections[group].size())
        throw std::runtime_error("Repository section index out of range");
    return sections[group][section].offset+stride[group]*instance;
}
std::uint16_t RnetDocument::crc16(const std::uint8_t* bytes,std::size_t size,std::uint16_t initial){
    auto crc=initial;for(std::size_t i=0;i<size;++i){crc^=std::uint16_t(bytes[i])<<8;for(unsigned bit=0;bit<8;++bit)crc=std::uint16_t((crc<<1)^((crc&0x8000)?0x1021:0));}return crc;
}
RnetDocument RnetDocument::read(const Bytes& bytes){
    if(bytes.size()<6||crc16(bytes.data(),6)!=0)throw std::runtime_error("Invalid R-net document header CRC");
    ArchiveReader a(bytes);RnetDocument d;d.header_version=a.u32();a.skip(2);
    if(d.header_version!=3)throw std::runtime_error("Only R-net document version 3 is currently supported");
    d.repository_version=a.u32();if(d.repository_version!=3&&d.repository_version!=4)throw std::runtime_error("Unsupported R-net repository version");
    auto notes=a.count();for(std::uint32_t i=0;i<notes;++i){auto start=a.position();d.comments.push_back(a.string());d.encoded_comments.emplace_back(bytes.begin()+start,bytes.begin()+a.position());}
    d.metadata=block(a);
    if(d.metadata.size()!=256)throw std::runtime_error("Unsupported R-net metadata block size");
    auto count=a.count();if(count>65535)throw std::runtime_error("Too many R-net repository files");
    std::set<std::uint64_t> keys;
    for(std::uint32_t i=0;i<count;++i){RepositoryFile f;f.id=a.u32();f.version=a.u32();f.field58=a.u16();f.field5a=a.u16();f.field5c=a.u32();f.data=block(a);
        if(!keys.insert(std::uint64_t(f.id)|(std::uint64_t(f.version)<<32)).second)throw std::runtime_error("Duplicate repository file key");
        d.files.push_back(std::move(f));
    }
    if(a.position()!=bytes.size())throw std::runtime_error("Unexpected trailing document data at "+std::to_string(a.position()));
    return d;
}
Bytes RnetDocument::write()const{
    if(header_version!=3||(repository_version!=3&&repository_version!=4)||metadata.size()!=256)throw std::runtime_error("Unsupported document state");
    Writer w;w.u32(header_version);auto crc=crc16(w.bytes.data(),w.bytes.size());w.u8(crc>>8);w.u8(crc&255);w.u32(repository_version);
    w.count(comments.size());for(std::size_t i=0;i<comments.size();++i){if(i<encoded_comments.size()&&ArchiveReader(encoded_comments[i]).string()==comments[i])w.bytes.insert(w.bytes.end(),encoded_comments[i].begin(),encoded_comments[i].end());else w.string(comments[i]);}w.block(metadata);w.count(files.size());
    for(auto&f:files){w.u32(f.id);w.u32(f.version);w.u16(f.field58);w.u16(f.field5a);w.u32(f.field5c);w.block(f.data);}return w.bytes;
}
}
