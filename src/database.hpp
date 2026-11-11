#pragma once
#include "archive.hpp"
#include <array>
#include <map>
namespace rnet {
struct ParameterDefinition {
    std::uint32_t key=0,id=0,extra84=0;
    std::array<std::u16string,6> texts;
    bool flag24=false;
    std::array<std::uint32_t,13> metadata{},serialized_metadata{};
    bool signed_storage=false;
    std::array<std::u16string,4> option_texts;
    bool flag70=false,flag71=false;
    std::array<std::uint32_t,4> extension{};
    std::u16string text88;
    std::uint32_t extra8c=0;
    bool flagb0=false;
    std::u16string text90;
    const std::u16string& name()const{return texts[2];}
    const std::u16string& help()const{return texts[5];}
    std::uint32_t storage_width()const{return metadata[12];}
};
struct StorageItem {std::uint32_t id=0,width=0;};
struct StorageSection {
    std::vector<StorageItem> items;
    std::array<std::uint32_t,4> fields{};
};
struct StorageGroup {
    std::array<StorageSection,7> sections;
    std::uint32_t instances=0,stride=0,last_section=0;
    std::array<std::uint32_t,7> offsets{};
};
struct StorageMap {
    std::uint32_t id=0,version=0;
    std::u16string name;
    std::array<StorageGroup,2> groups;
};
struct PredefinedEditor {
    std::uint32_t id=0,flags=0,kind=0,count=0,extra=0;
    std::u16string name;
    bool mapped=false;
    std::array<std::u16string,33> labels;
    std::array<std::uint32_t,33> values{};
};
class ParameterCatalog {
public:
    std::uint32_t schema=0;
    std::size_t parameter_offset=0,parameter_end=0;
    std::map<std::uint32_t,ParameterDefinition> parameters;
    std::map<std::uint32_t,PredefinedEditor> editors;
    std::map<std::uint64_t,StorageMap> storage_maps;
    std::map<std::uint32_t,std::u16string> file_names;
    std::size_t storage_map_offset=0,storage_map_end=0;
    static ParameterCatalog read(const Bytes& plain);
    // OEM schema-33 mode observed in the original: file attribute bit 15 is
    // omitted for map lookup (00603020, DAT_006cda1c == 0). Raw IDs stay intact.
    static std::uint32_t storage_id(std::uint32_t id){return id&~std::uint32_t(0x8000);}
    void normalize_access(std::uint32_t level);
};
}
