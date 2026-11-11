#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
namespace rnet {
using Bytes=std::vector<std::uint8_t>;
Bytes read_file(const std::filesystem::path& path);
struct Resource {std::string type, name; std::uint32_t language; Bytes bytes;};
struct MenuItem {std::uint16_t flags=0, id=0; std::u16string text; std::vector<MenuItem> children;};
struct TextResource {std::uint32_t id; std::u16string text;};
struct Import {std::string dll; std::vector<std::string> symbols;};
struct Section {std::uint32_t address,size,offset,raw_size;};
class PeImage {
    Bytes data_;
    std::vector<Section> sections_;
    std::size_t optional_=0;
    std::uint16_t word(std::size_t p) const;
    std::uint32_t dword(std::size_t p) const;
    std::string ascii(std::size_t p) const;
    std::size_t offset(std::uint32_t address) const;
public:
    explicit PeImage(Bytes data);
    std::uint32_t entry_point() const;
    Bytes read_va(std::uint32_t va,std::size_t size) const;
    std::vector<Resource> resources() const;
    std::vector<Import> imports() const;
    std::vector<TextResource> strings() const;
    static std::vector<MenuItem> menu(const Bytes& bytes);
};
std::string utf8(const std::u16string& text);
}
