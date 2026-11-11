#include "values.hpp"
#include "runtime_resources.hpp"
#include <iostream>
#include <map>
#include <fstream>
#include <algorithm>
std::string text(rnet::ArchiveReader& a){auto n=a.u32();if(n>4096)throw std::runtime_error("Reference name too long");std::string s;while(n--)s.push_back(char(a.u8()));return s;}
int main(int argc,char**argv){try{
    if(argc!=6)throw std::runtime_error("Original EXE, RND, fixtures, original x86 reference and edited output required");
    auto catalog=rnet::bundled_catalog();
    auto reference=rnet::read_file(argv[4]);rnet::ArchiveReader r(reference);if(r.u32()!=0x31564e52)throw std::runtime_error("Unsupported value reference");
    auto documents=r.u32();if(documents!=6)throw std::runtime_error("Expected default and five user documents");
    unsigned total=0;std::map<std::string,std::vector<rnet::DocumentValue>> decoded;
    for(unsigned di=0;di<documents;++di){
        auto name=text(r);r.skip(32);auto path=std::filesystem::path(argv[3]);if(di)path/="user-documents";path/=std::filesystem::u8path(name);
        auto bytes=rnet::read_file(path);auto doc=rnet::RnetDocument::read(bytes);if(doc.write()!=bytes)throw std::runtime_error("User document roundtrip changed bytes");
        std::vector<std::string> warnings;auto values=rnet::read_values(doc,catalog,&warnings);
        if(warnings.size()!=(di?1u:0u)||(di&&warnings[0].find("65557 version 7")==std::string::npos))throw std::runtime_error("Unknown document version reporting differs");
        auto count=r.u32();if(count!=values.size())throw std::runtime_error("Original x86 value count differs");
        for(const auto& value:values){
            const std::array<std::uint32_t,9> fields{std::uint32_t(value.file),value.parameter,value.group,value.profile,value.section,value.element,std::uint32_t(value.offset),value.width,value.numeric?1u:0u};
            for(auto expected:fields)if(r.u32()!=expected)throw std::runtime_error("Original x86 parameter offset differs in "+name);
            auto raw=r.u32();if(value.numeric&&raw!=std::uint32_t(value.number))throw std::runtime_error("Original x86 numeric getter differs");
            ++total;
        }
        auto corrupt=doc;corrupt.files[0].data.back()^=1;bool rejected=false;
        try{rnet::read_values(corrupt,catalog,&warnings);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Payload corruption accepted");
        if(di){rejected=false;try{rnet::read_values(doc,catalog);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("Unknown file version silently accepted");}
        decoded.emplace(name,std::move(values));std::cout<<name<<": "<<count<<" values match original x86, "<<warnings.size()<<" unknown versions reported\n";
    }
    if(r.position()!=reference.size())throw std::runtime_error("Trailing x86 reference bytes");
    auto find=[&](const std::string& name,unsigned parameter)->const rnet::DocumentValue&{
        for(const auto& v:decoded.at(name))if(v.parameter==parameter&&v.group==1&&v.profile==0)return v;
        throw std::runtime_error("Missing profile parameter");
    };
    if(find("CS16110708.R-net",312).number!=80||find("CS16110708geänder.R-net",312).number!=86||find("CS16110708geänder.R-net",318).number!=30)throw std::runtime_error("Known user speed changes differ");
    if(find("CS16110708-indoor.R-net",306).text!=u"indoor"||find("CS16110708geänder.R-net",306).text!=u"aussen")throw std::runtime_error("Known user profile names differ");
    const auto source_path=std::filesystem::path(argv[3])/"user-documents"/"CS16110708.R-net";
    const auto source=rnet::read_file(source_path);auto edited=rnet::RnetDocument::read(source);
    // Exercise the editing boundary, including rejected calls that bypass GUI
    // enablement. Use a real mapped numeric value with controlled access metadata.
    auto access_catalog=catalog;
    for(unsigned required=0;required<=3;++required){
        auto& flags=access_catalog.parameters.at(312).metadata[1];flags=(flags&31)|(required<<5);
        for(unsigned selected=0;selected<=3;++selected){
            auto candidate=rnet::RnetDocument::read(source);bool denied=false;
            try{rnet::edit_value(candidate,access_catalog,find("CS16110708.R-net",312),u"86",static_cast<rnet::AccessLevel>(selected));}
            catch(const std::runtime_error&){denied=true;}
            if(denied!=(selected<required))throw std::runtime_error("Dongle access selection did not gate the editing API");
            if(denied&&candidate.write()!=source)throw std::runtime_error("Denied access changed document bytes");
            if(!denied&&candidate.write()==source)throw std::runtime_error("Authorized edit was not applied");
        }
    }
    for(auto selected:{rnet::AccessLevel::OEM,static_cast<rnet::AccessLevel>(4),static_cast<rnet::AccessLevel>(~0u)}){
        auto candidate=rnet::RnetDocument::read(source);bool denied=false;
        try{rnet::edit_value(candidate,access_catalog,find("CS16110708.R-net",312),u"86",selected);}catch(const std::runtime_error&){denied=true;}
        if(!denied||candidate.write()!=source)throw std::runtime_error("OEM or invalid selection granted Factory access");
    }
    std::cout<<"PASS: all four Dongle access levels gate real document editing; denied calls preserve every byte\n";
    for(const auto& invalid:{u"0",u"101",u"86 km/h",u"-1",u"999999999999999999999"}){
        bool rejected=false;try{rnet::edit_value(edited,catalog,find("CS16110708.R-net",312),invalid);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||edited.write()!=source)throw std::runtime_error("Invalid numeric edit changed document");
    }
    bool rejected=false;try{rnet::edit_value(edited,catalog,find("CS16110708.R-net",306),u"123456789012345678901");}catch(const std::runtime_error&){rejected=true;}
    if(!rejected||edited.write()!=source)throw std::runtime_error("Overlong text changed document");
    {
        auto boolean_doc=rnet::RnetDocument::read(source);std::vector<std::string> warning;
        auto all=rnet::read_values(boolean_doc,catalog,&warning);
        auto it=std::find_if(all.begin(),all.end(),[](const auto& v){return v.parameter==68;});
        if(it==all.end()||it->width!=1||catalog.parameters.at(68).extension[0]!=1)throw std::runtime_error("PID68 boolean mapping missing");
        auto before=boolean_doc.write();
        for(const auto& invalid:{u"-1",u"2",u"true"}){
            bool denied=false;try{rnet::edit_value(boolean_doc,catalog,*it,invalid);}catch(const std::runtime_error&){denied=true;}
            if(!denied||boolean_doc.write()!=before)throw std::runtime_error("Invalid boolean changed file");
        }
        if(it->number!=1&&!rnet::edit_value(boolean_doc,catalog,*it,u"1"))throw std::runtime_error("Boolean 1 edit missing");
        auto after=rnet::read_values(boolean_doc,catalog,&warning);unsigned differences=0;
        for(std::size_t i=0;i<all.size();++i)if(all[i].number!=after[i].number||all[i].text!=after[i].text){if(all[i].parameter!=68)throw std::runtime_error("Boolean edit changed another parameter");++differences;}
        if(differences!=unsigned(it->number!=1))throw std::runtime_error("Boolean edit count differs");
        if(it->number!=1){rnet::edit_value(boolean_doc,catalog,*it,std::u16string(1,char16_t('0'+it->number)));if(boolean_doc.write()!=before)throw std::runtime_error("Boolean roundtrip changed bytes");}
    }
    auto original=rnet::RnetDocument::read(source);
    if(!rnet::edit_value(edited,catalog,find("CS16110708.R-net",312),u"86")||!rnet::edit_value(edited,catalog,find("CS16110708.R-net",306),u"indoor"))throw std::runtime_error("Expected edits were not applied");
    std::vector<std::string> warnings;auto changed=rnet::read_values(edited,catalog,&warnings);
    unsigned changes=0;
    const auto& before=decoded.at("CS16110708.R-net");
    for(std::size_t i=0;i<before.size();++i){
        if(before[i].number!=changed[i].number||before[i].text!=changed[i].text){
            if((changed[i].parameter!=312&&changed[i].parameter!=306)||changed[i].profile!=0||changed[i].group!=1)throw std::runtime_error("Edit changed another parameter");
            ++changes;
        }
    }
    if(changes!=2||edited.metadata!=original.metadata)throw std::runtime_error("Edited document contents differ");
    for(std::size_t i=0;i<original.files.size();++i)if(original.files[i].id!=0x10001&&original.files[i].id!=0x10006&&original.files[i].data!=edited.files[i].data)throw std::runtime_error("Edit changed unrelated file");
    const auto encoded=edited.write();if(rnet::RnetDocument::read(encoded).write()!=encoded)throw std::runtime_error("Edited roundtrip differs");
    auto original_resaved=rnet::RnetDocument::read(rnet::read_file(std::filesystem::path(argv[3])/"original-resaved-edited.R-net"));
    warnings.clear();const auto resaved_values=rnet::read_values(original_resaved,catalog,&warnings);
    if(resaved_values.size()!=changed.size())throw std::runtime_error("Original-resaved value count differs");
    for(const auto& expected:changed){
        bool matched=false;
        for(const auto& actual:resaved_values){
            if(edited.files[expected.file].id==original_resaved.files[actual.file].id&&expected.parameter==actual.parameter&&expected.group==actual.group&&expected.profile==actual.profile&&expected.section==actual.section&&expected.element==actual.element){
                if(expected.number!=actual.number||expected.text!=actual.text)throw std::runtime_error("Original re-save changed a mapped value");
                matched=true;break;
            }
        }
        if(!matched)throw std::runtime_error("Edited value missing after original re-save");
    }
    if(std::filesystem::exists(argv[5])&&std::filesystem::equivalent(source_path,argv[5]))throw std::runtime_error("Refuse original overwrite");
    std::ofstream output(argv[5],std::ios::binary);if(!output.write(reinterpret_cast<const char*>(encoded.data()),encoded.size()))throw std::runtime_error("Edited output failed");
    std::cout<<"PASS: validated numeric/text edits affect only selected values and group checksums; unknown Servo payload and metadata preserved\n";
    std::cout<<"PASS: all 2026 edited document values survive opening and re-saving in isolated original\n";
    std::cout<<"PASS: "<<total<<" original-x86 offset/getter comparisons; six lossless roundtrips; known profile/speed changes and content checksums verified\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
