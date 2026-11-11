#include "values.hpp"
#include "can_writer.hpp"
#include <set>
#include <algorithm>
namespace rnet {
namespace {
std::uint16_t checksum(const Bytes& b,std::size_t offset,std::size_t size){
    std::uint32_t sum=0;for(std::size_t i=offset;i<offset+size;++i)sum+=b.at(i);return std::uint16_t(~sum);
}
}
std::vector<DocumentValue> read_values(const RnetDocument& doc,const ParameterCatalog& catalog,std::vector<std::string>* warnings){
    std::vector<DocumentValue> result;
    for(std::size_t fi=0;fi<doc.files.size();++fi){
        const auto& file=doc.files[fi];const auto layout=RepositoryLayout::read(file.data);
        for(unsigned gi=0;gi<2;++gi){
            const auto group_start=layout.header_size+(gi?layout.stride[0]:0);
            for(unsigned profile=0;profile<(gi?8u:1u)&&layout.stride[gi];++profile){
                const auto start=group_start+profile*layout.stride[gi];
                if(layout.stride[gi]<2||checksum(file.data,start+2,layout.stride[gi]-2)!=(std::uint16_t(file.data[start])|(std::uint16_t(file.data[start+1])<<8)))throw std::runtime_error("Repository content checksum failed");
            }
        }
        const auto key=std::uint64_t(ParameterCatalog::storage_id(file.id))|(std::uint64_t(file.version)<<32);
        auto found=catalog.storage_maps.find(key);
        if(found==catalog.storage_maps.end()){
            auto message="No verified storage map for repository file "+std::to_string(file.id)+" version "+std::to_string(file.version);
            if(!warnings)throw std::runtime_error(message);
            warnings->push_back(message);continue;
        }
        const auto& map=found->second;
        for(unsigned gi=0;gi<2;++gi){
            const auto& group=map.groups[gi];
            if(group.instances!=(gi?8u:1u)||group.stride!=layout.stride[gi])throw std::runtime_error("Storage group does not match repository payload");
            std::size_t group_start=layout.header_size+(gi?layout.stride[0]:0);
            unsigned present=0;
            for(unsigned si=0;si<7;++si){
                const auto& section=group.sections[si];
                if(section.items.size()!=section.fields[0]||section.fields[3]!=std::uint64_t(section.fields[1])*section.fields[2])throw std::runtime_error("Inconsistent storage section");
                std::size_t item_stride=0;for(const auto& item:section.items)item_stride+=item.width;
                if(item_stride!=section.fields[1])throw std::runtime_error("Inconsistent storage item stride");
                if(!section.fields[3])continue;
                if(present>=layout.sections[gi].size())throw std::runtime_error("Repository section missing");
                const auto& actual=layout.sections[gi][present++];
                if(actual.instances!=section.fields[2]||actual.stride!=section.fields[1]||actual.offset!=group_start+group.offsets[si])throw std::runtime_error("Repository section layout differs");
                for(unsigned profile=0;profile<group.instances;++profile){
                    for(unsigned element=0;element<section.fields[2];++element){
                        std::size_t offset=actual.offset+profile*group.stride+element*item_stride;
                        for(const auto& item:section.items){
                            const auto parameter=item.id&0xffff;auto definition=catalog.parameters.find(parameter);
                            if(offset>file.data.size()||item.width>file.data.size()-offset)throw std::runtime_error("Stored value outside payload");
                            if(parameter!=0x2f2&&parameter!=0x26b&&definition!=catalog.parameters.end()){
                                const auto& p=definition->second;
                                if(p.storage_width()!=item.width)throw std::runtime_error("Parameter storage width differs");
                                DocumentValue value;value.file=fi;value.offset=offset;value.parameter=parameter;value.width=item.width;value.group=gi;value.profile=profile;value.section=si;value.element=element;
                                // Original 005cf060 distinguishes text using zero bounds
                                // and zero numeric metadata; 005bffd0 supports 1/2/4 bytes.
                                value.numeric=p.metadata[4]!=0||p.metadata[5]!=0||p.metadata[9]!=0;
                                if(value.numeric){
                                    if(item.width!=1&&item.width!=2&&item.width!=4)throw std::runtime_error("Unsupported numeric width");
                                    std::uint32_t raw=0;for(unsigned j=0;j<item.width;++j)raw|=std::uint32_t(file.data[offset+j])<<(8*j);
                                    value.number=raw;
                                    if(p.signed_storage&&(raw&(std::uint32_t(1)<<(8*item.width-1))))value.number-=std::int64_t(1)<<(8*item.width);
                                }else{
                                    if(item.width>=255)throw std::runtime_error("Unsupported text storage width");
                                    Bytes encoded{std::uint8_t(item.width)};encoded.insert(encoded.end(),file.data.begin()+offset,file.data.begin()+offset+item.width);
                                    value.text=ArchiveReader(encoded).string();auto end=value.text.find(char16_t(0));if(end!=std::u16string::npos)value.text.resize(end);
                                    while(!value.text.empty()&&value.text.back()==u' ')value.text.pop_back();
                                }
                                result.push_back(std::move(value));
                            }
                            offset+=item.width;
                        }
                    }
                }
            }
            if(present!=layout.sections[gi].size())throw std::runtime_error("Unexpected repository section");
        }
    }
    return result;
}
std::vector<ParameterChoice> parameter_choices(const ParameterDefinition &p,const DocumentValue &v,const ParameterCatalog &catalog) {
    auto found=catalog.editors.find(p.extension[0]);
    if(!v.numeric || (v.width!=1&&v.width!=2&&v.width!=4) || found==catalog.editors.end())return {};
    const auto &editor=found->second;
    if(editor.kind!=4 || !editor.mapped || editor.count==0 || editor.count>33 ||
       p.metadata[8]!=1 || p.metadata[9]!=1 || p.metadata[10] || p.metadata[11])return {};
    const std::int64_t minimum=p.signed_storage?std::int32_t(p.metadata[4]):std::int64_t(p.metadata[4]);
    const std::int64_t maximum=p.signed_storage?std::int32_t(p.metadata[5]):std::int64_t(p.metadata[5]);
    const unsigned bits=v.width*8;
    const std::uint64_t mask=(std::uint64_t(1)<<bits)-1;
    std::vector<ParameterChoice> choices;std::set<std::int64_t> seen;
    for(unsigned i=0;i<editor.count;++i){
        const auto raw=editor.values[i];std::int64_t number=raw;
        if(std::uint64_t(raw)>mask){
            if(!p.signed_storage || std::int32_t(raw)>=0 || std::int32_t(raw)<-(std::int64_t(1)<<(bits-1)))return {};
            number=std::int32_t(raw);
        }else if(p.signed_storage && (raw&(std::uint64_t(1)<<(bits-1))))number-=std::int64_t(1)<<bits;
        if(number<minimum || number>maximum || (p.metadata[6]&&(number-minimum)%p.metadata[6]))continue;
        if(editor.labels[i].empty() || !seen.insert(number).second)return {};
        choices.push_back({number,editor.labels[i]});
    }
    return choices;
}
bool supports_direct_edit(const ParameterDefinition& p,const DocumentValue& value,const ParameterCatalog *catalog){
    if(!value.numeric)return p.extension[0]==17;
    const bool boolean=p.extension[0]==1&&!p.signed_storage&&p.metadata[4]==0&&p.metadata[5]==1&&p.metadata[6]==1&&value.width==1;
    const bool enumeration=catalog&&!parameter_choices(p,value,*catalog).empty();
    return (p.extension[0]==13||boolean||enumeration)&&p.metadata[8]==1&&p.metadata[9]==1&&!p.metadata[10]&&!p.metadata[11];
}
Bytes encode_parameter_value(const ParameterDefinition &p,const DocumentValue &value,const std::u16string &text,const ParameterCatalog &catalog){
    Bytes encoded;
    if(value.numeric){
        if(!supports_direct_edit(p,value,&catalog))throw std::runtime_error("This numeric conversion is not yet supported for editing");
        auto number_text=utf8(text);if(number_text.empty())throw std::runtime_error("Enter an integer");
        std::size_t start=number_text[0]=='-'?1:0;if(start==number_text.size())throw std::runtime_error("Enter an integer");
        for(std::size_t i=start;i<number_text.size();++i)if(number_text[i]<'0'||number_text[i]>'9')throw std::runtime_error("Enter an integer without units");
        std::int64_t number;try{number=std::stoll(number_text);}catch(const std::exception&){throw std::runtime_error("Integer is out of range");}
        const std::int64_t minimum=p.signed_storage?std::int64_t(std::int32_t(p.metadata[4])):p.metadata[4];
        const std::int64_t maximum=p.signed_storage?std::int64_t(std::int32_t(p.metadata[5])):p.metadata[5];
        if(number<minimum||number>maximum)throw std::runtime_error("Value must be between "+std::to_string(minimum)+" and "+std::to_string(maximum));
        const auto step=p.metadata[6];if(step&&(number-minimum)%step)throw std::runtime_error("Value does not match the parameter step");
        const auto choices=parameter_choices(p,value,catalog);
        if(!choices.empty() && std::none_of(choices.begin(),choices.end(),[&](const auto &choice){return choice.value==number;}))
            throw std::runtime_error("Select one of the listed values");
        const auto bits=8*value.width;const auto storage_min=p.signed_storage?-(std::int64_t(1)<<(bits-1)):0;
        const auto storage_max=p.signed_storage?(std::int64_t(1)<<(bits-1))-1:(std::int64_t(1)<<bits)-1;
        if(number<storage_min||number>storage_max)throw std::runtime_error("Value exceeds storage width");
        for(unsigned j=0;j<value.width;++j)encoded.push_back(std::uint8_t(std::uint64_t(number)>>(8*j)));
    }else{
        if(p.extension[0]!=17)throw std::runtime_error("This text type is not yet supported for editing");
        if(text.size()>value.width)throw std::runtime_error("Text is too long for this parameter");
        for(auto character:text){
            if(character<32||(character>=127&&character<=159))throw std::runtime_error("Control characters are not allowed");
            bool found=false;
            for(unsigned byte=32;byte<256;++byte){Bytes test{1,std::uint8_t(byte)};if(ArchiveReader(test).string()[0]==character){encoded.push_back(std::uint8_t(byte));found=true;break;}}
            if(!found)throw std::runtime_error("Character cannot be stored as Windows-1252");
        }
        encoded.resize(value.width,' ');
    }
    return encoded;
}
bool edit_value(RnetDocument& doc,const ParameterCatalog& catalog,const DocumentValue& requested,const std::u16string& text,AccessLevel level){
    std::vector<std::string> warnings;const auto values=read_values(doc,catalog,&warnings);
    const DocumentValue* current=nullptr;
    for(const auto& value:values)if(value.file==requested.file&&value.parameter==requested.parameter&&value.group==requested.group&&value.profile==requested.profile&&value.section==requested.section&&value.element==requested.element){current=&value;break;}
    if(!current)throw std::runtime_error("Parameter is not mapped in this document");
    const auto& value=*current;const auto& p=catalog.parameters.at(value.parameter);
    if(!access_allows(p.metadata[1]>>5,level))throw std::runtime_error("Parameter exceeds selected Dongle access level");
    auto encoded=encode_parameter_value(p,value,text,catalog);
    auto next=doc;auto& data=next.files[value.file].data;
    if(std::equal(encoded.begin(),encoded.end(),data.begin()+value.offset))return false;
    std::copy(encoded.begin(),encoded.end(),data.begin()+value.offset);
    const auto layout=RepositoryLayout::read(data);
    const auto group_start=layout.header_size+(value.group?layout.stride[0]:0)+value.profile*layout.stride[value.group];
    // Original 005c1030: each common/profile group has a leading checksum word.
    const auto sum=checksum(data,group_start+2,layout.stride[value.group]-2);
    data[group_start]=std::uint8_t(sum);data[group_start+1]=std::uint8_t(sum>>8);
    warnings.clear();read_values(next,catalog,&warnings);doc=std::move(next);return true;
}
std::vector<std::size_t> controller_write_plan(const RnetDocument &before,
    const RnetDocument &after, const ParameterCatalog &catalog, AccessLevel level) {
  if (before.files.size()!=after.files.size()) throw std::runtime_error("Repository file set changed");
  std::set<unsigned> ids;
  for (std::size_t i=0;i<before.files.size();++i) {
    if (!ids.insert(before.files[i].id).second || before.files[i].id!=after.files[i].id ||
        before.files[i].version!=after.files[i].version || before.files[i].data.size()!=after.files[i].data.size())
      throw std::runtime_error("Repository file identity changed");
  }
  std::vector<std::string> warnings;
  auto oldValues=read_values(before,catalog,&warnings);
  auto newValues=read_values(after,catalog,&warnings);
  if (oldValues.size()!=newValues.size()) throw std::runtime_error("Mapped value set changed");
  auto replay=before;
  for (std::size_t i=0;i<oldValues.size();++i) {
    const auto &a=oldValues[i], &b=newValues[i];
    if (a.file!=b.file || a.offset!=b.offset || a.parameter!=b.parameter || a.width!=b.width)
      throw std::runtime_error("Parameter mapping changed");
    if (a.number!=b.number || a.text!=b.text) {
      auto value=b.text;
      if (b.numeric) {auto number=std::to_string(b.number);value.assign(number.begin(),number.end());}
      edit_value(replay,catalog,a,value,level);
    }
  }
  if (replay.write()!=after.write()) throw std::runtime_error("Unsupported or unmapped byte changes; controller write refused");
  std::vector<std::size_t> files;
  for (std::size_t i=0;i<before.files.size();++i) if (before.files[i].data!=after.files[i].data) {
    validate_repository_change(before.files[i],after.files[i]);files.push_back(i);
  }
  return files;
}

}
