#pragma once
#include "archive.hpp"
#include <array>
namespace rnet {
struct RepositoryFile {
    std::uint32_t id=0,version=0;
    std::uint16_t field58=0,field5a=0;
    std::uint32_t field5c=0;
    Bytes data;
};
struct RepositorySection {
    std::uint8_t instances=0,stride=0;
    std::size_t offset=0;
    std::size_t size()const{return std::size_t(instances)*stride;}
};
// Read-only interpretation of the modern payload header. Legacy payloads need
// the original migration rules and are deliberately rejected here.
struct RepositoryLayout {
    std::uint16_t checksum=0;
    std::size_t header_size=0;
    std::array<std::vector<RepositorySection>,2> sections;
    std::array<std::size_t,2> stride{};
    static RepositoryLayout read(const Bytes& data);
    std::size_t offset(unsigned group,unsigned instance,unsigned section)const;
};
// Version-3 R-net document. Unknown fields and encrypted metadata are preserved.
// No controller operations or parameter writes are performed by this class.
class RnetDocument {
public:
    std::uint32_t header_version=0,repository_version=0;
    std::vector<std::u16string> comments;
    std::vector<Bytes> encoded_comments;
    Bytes metadata;
    std::vector<RepositoryFile> files;
    static RnetDocument read(const Bytes& bytes);
    Bytes write()const;
    static std::uint16_t crc16(const std::uint8_t* bytes,std::size_t size,std::uint16_t initial=0xffff);
};
}
