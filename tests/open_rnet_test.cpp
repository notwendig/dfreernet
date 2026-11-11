#include "values.hpp"
#include "runtime_resources.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc,char**argv){try{
    if(argc!=5)throw std::runtime_error("Original EXE, RND, fixture directory and --record/--verify required");
    auto root=std::filesystem::path(argv[3]);bool record=std::string(argv[4])=="--record";
    if(!record&&std::string(argv[4])!="--verify")throw std::runtime_error("Unknown mode");
    auto catalog=rnet::bundled_catalog();
    std::map<std::string,std::array<std::size_t,4>> expected;
    if(!record){
        std::ifstream input(root/"corpus.tsv");if(!input)throw std::runtime_error("Missing private corpus manifest");
        std::string line;while(std::getline(input,line)){std::istringstream row(line);std::string name;std::getline(row,name,'\t');std::array<std::size_t,4> counts{};for(auto& n:counts)if(!(row>>n))throw std::runtime_error("Invalid corpus manifest");if(!expected.emplace(name,counts).second)throw std::runtime_error("Duplicate corpus entry");}
    }
    std::vector<std::filesystem::path> paths;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root/"documents"))if(entry.is_regular_file()&&entry.path().extension()==".R-net")paths.push_back(entry.path());
    std::sort(paths.begin(),paths.end());if(paths.size()!=27||(!record&&expected.size()!=paths.size()))throw std::runtime_error("Expected all 27 source documents");
    std::size_t total_files=0,total_values=0,total_opaque=0;bool alias_edit_verified=false;
    for(const auto& path:paths){
        auto name=std::filesystem::relative(path,root/"documents").generic_u8string();auto bytes=rnet::read_file(path);auto document=rnet::RnetDocument::read(bytes);
        if(document.write()!=bytes)throw std::runtime_error("Corpus roundtrip changed bytes: "+name);
        std::vector<std::string> warnings;auto values=rnet::read_values(document,catalog,&warnings);
        const std::array<std::size_t,4> counts{bytes.size(),document.files.size(),values.size(),warnings.size()};
        if(record){std::cout<<name;for(auto count:counts)std::cout<<'\t'<<count;std::cout<<'\n';}
        else if(expected.at(name)!=counts)throw std::runtime_error("Corpus coverage changed: "+name);
        // Attribute-bearing IDs are preserved on serialization and may use the
        // original's canonical map. Unknown versions remain explicitly opaque.
        for(const auto& value:values){auto& file=document.files[value.file];auto key=std::uint64_t(rnet::ParameterCatalog::storage_id(file.id))|(std::uint64_t(file.version)<<32);if(!catalog.storage_maps.count(key))throw std::runtime_error("Value decoded without an exact-version map");}
        if(!alias_edit_verified)for(const auto& value:values){
            const auto raw_id=document.files[value.file].id;
            if((raw_id&0x8000)&&value.parameter==312&&value.group==1&&value.profile==0){
                auto changed=document;const auto& p=catalog.parameters.at(value.parameter);const auto replacement=value.number==p.metadata[4]?p.metadata[5]:p.metadata[4];
                auto input=std::to_string(replacement);
                if(!rnet::edit_value(changed,catalog,value,std::u16string(input.begin(),input.end())))throw std::runtime_error("Attribute-bearing file edit was not applied");
                auto reopened=rnet::RnetDocument::read(changed.write());
                if(reopened.files[value.file].id!=raw_id||document.files[value.file].data==reopened.files[value.file].data)throw std::runtime_error("Canonical lookup changed the stored file ID");
                std::vector<std::string> ignored;auto after=rnet::read_values(reopened,catalog,&ignored);bool found=false;
                for(const auto& candidate:after)if(candidate.file==value.file&&candidate.parameter==value.parameter&&candidate.group==value.group&&candidate.profile==value.profile&&candidate.element==value.element&&candidate.section==value.section){if(candidate.number!=replacement)throw std::runtime_error("Attribute-bearing file edit roundtrip differs");found=true;}
                if(!found||document.write()!=bytes)throw std::runtime_error("Alias edit changed the source document");
                alias_edit_verified=true;break;
            }
        }
        total_files+=document.files.size();total_values+=values.size();total_opaque+=warnings.size();
    }
    if(!alias_edit_verified)throw std::runtime_error("Real attribute-bearing speed edit was not exercised");
    std::cerr<<"PASS: 27 real documents roundtrip exactly; "<<total_files<<" files, "<<total_values<<" mapped values, "<<total_opaque<<" opaque file versions reported; all modern payload checksums verified; alias edit preserves raw ID\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
