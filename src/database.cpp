#include "database.hpp"
namespace rnet {
ParameterCatalog ParameterCatalog::read(const Bytes& bytes){
    ArchiveReader a(bytes);ParameterCatalog result;result.schema=a.u32();
    if(result.schema!=33)throw std::runtime_error("Parameter catalog currently supports RND schema 33");
    auto strings=a.count();for(std::uint32_t i=0;i<strings;++i)a.string();
    a.skip(a.count()); // CSlotItems' serialized CByteArray, not a table of parameters.
    if(a.u32()!=1)throw std::runtime_error("Unsupported organization schema");
    auto organizations=a.count();for(std::uint32_t i=0;i<organizations;++i){a.u16();for(int j=0;j<20;++j)a.string();}
    // CDepMap -> CDependencyList -> CDependencyItem (005dcc10 / 005dcf60).
    auto dependencies=a.count();
    for(std::uint32_t i=0;i<dependencies;++i){
        a.u32();if(a.u32()!=1)throw std::runtime_error("Unsupported dependency list schema");
        a.boolean();auto items=a.count();a.skip(std::size_t(items)*8);
    }
    a.boolean();
    // CNVLayoutList / CNVLayoutItem (005fbbb0 / 005bc600).
    if(a.u32()!=1)throw std::runtime_error("Unsupported file layout schema");
    a.boolean();auto layouts=a.count();
    for(std::uint32_t i=0;i<layouts;++i){
        auto file_id=a.u32();a.u32();auto file_name=a.string();result.file_names.emplace(file_id,std::move(file_name));
        a.boolean();a.u32();a.boolean();auto columns=a.u32();
        if(columns>10000)throw std::runtime_error("Too many layout columns");
        for(std::uint32_t j=0;j<columns;++j)a.string();
        a.skip(9*4);
    }
    result.parameter_offset=a.position();
    // CParameterInfo / CParameterItem (005b6360 / 0059a8f0).
    if(a.u32()!=1)throw std::runtime_error("Unsupported parameter schema at "+std::to_string(result.parameter_offset));
    a.boolean();a.u32();auto count=a.count();
    for(std::uint32_t i=0;i<count;++i){
        const auto key=a.u32();ParameterDefinition p;p.key=key;p.id=a.u32();p.extra84=a.u32();
        for(auto& text:p.texts)text=a.string();
        p.flag24=a.boolean();for(auto& v:p.metadata)v=a.u32();p.serialized_metadata=p.metadata;
        p.signed_storage=a.boolean();for(auto& text:p.option_texts)text=a.string();
        p.flag70=a.boolean();p.flag71=a.boolean();for(auto& v:p.extension)v=a.u32();
        p.text88=a.string();p.extra8c=a.u32();p.flagb0=a.boolean();p.text90=a.string();
        if(!result.parameters.emplace(key,std::move(p)).second)throw std::runtime_error("Duplicate parameter key");
    }
    result.parameter_end=a.position();
    // CPreDefEditMap, schema-3 edits (005fb180 / 00597d90 / 00597ae0).
    // Original serializers 00597ae0 / 00597d90: 33 labels and explicit
    // raw values, not sequential combo-box indices.
    if(a.u32()!=1)throw std::runtime_error("Unsupported predefined edit schema");
    auto edits=a.count();
    for(std::uint32_t i=0;i<edits;++i){
        auto key=a.u32();if(a.u32()!=3)throw std::runtime_error("Unsupported predefined edit item version");
        PredefinedEditor editor;
        editor.id=a.u32();editor.flags=a.u32();editor.name=a.string();
        editor.kind=a.u32();editor.count=a.u32();editor.mapped=a.boolean();
        for(auto &label:editor.labels)label=a.string();
        for(auto &value:editor.values)value=a.u32();
        editor.extra=a.u32();
        if(editor.id!=key || editor.count>33 || !result.editors.emplace(key,std::move(editor)).second)
            throw std::runtime_error("Invalid predefined editor definition");
    }
    // CSystemMap -> CParameterMap (00602c90 / 00418c30 / 005b3620).
    result.storage_map_offset=a.position();
    if(a.u32()!=1)throw std::runtime_error("Unsupported storage map schema");
    a.boolean();auto maps=a.count();
    for(std::uint32_t i=0;i<maps;++i){
        StorageMap map;map.id=a.u32();map.version=a.u32();
        for(auto& group:map.groups){
            // 005fea30; section arrays: 006025d0 -> 005f5830.
            for(auto& section:group.sections){
                auto items=a.count();if(items>65535)throw std::runtime_error("Too many storage items");
                for(std::uint32_t j=0;j<items;++j){StorageItem item;item.id=a.u32();item.width=a.u32();section.items.push_back(item);}
                for(auto& field:section.fields)field=a.u32();
            }
            group.instances=a.u32();group.stride=a.u32();group.last_section=a.u32();
            for(auto& offset:group.offsets)offset=a.u32();
        }
        if(a.u32()!=map.id||a.u32()!=map.version)throw std::runtime_error("Storage map identity differs");
        map.name=a.string();a.boolean();a.skip(8);a.boolean();a.boolean();a.skip(8);a.boolean();
        // CDescriptor (005dc190): checksum, header size, counts/strides,
        // then CSsegInfoList's byte pairs; not a fixed-size opaque block.
        a.u16();a.u32();for(unsigned j=0;j<2;++j){a.u8();a.u32();}
        auto segments=a.count();a.skip(std::size_t(segments)*2);
        const auto key=std::uint64_t(map.id)|(std::uint64_t(map.version)<<32);
        if(!result.storage_maps.emplace(key,std::move(map)).second)throw std::runtime_error("Duplicate storage map key");
    }
    result.storage_map_end=a.position();
    // Original post-load path calls 005b4fc0 with level 2 (caller 005954a5).
    result.normalize_access(2);
    return result;
}
void ParameterCatalog::normalize_access(std::uint32_t level){
    // Confirmed by original code and a GDB hardware watchpoint, no hardcoded IDs.
    for(auto& entry:parameters){auto& p=entry.second;
        if((p.metadata[1]>>5)>level&&(p.metadata[3]>>5)<=level){
            p.metadata[1]=(p.metadata[3]>>5)<<5;
            p.metadata[0]=p.metadata[1]+31;
            p.flag70=true;
        }
    }
}
}
