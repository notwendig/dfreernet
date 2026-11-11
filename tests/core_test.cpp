#include "pe_image.hpp"
#include <iostream>
#include <algorithm>
#include <functional>
#include <fstream>
using namespace rnet;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Original PE path required");
    PeImage p(read_file(argv[1]));require(p.entry_point()==0x42b160,"Original entry point changed");
    auto resources=p.resources();require(resources.size()==585,"Resource inventory differs from independent analysis");
    auto strings=p.strings();require(strings.size()==1454,"String inventory differs");
    auto found=std::find_if(strings.begin(),strings.end(),[](auto& s){return s.id==586;});
    require(found!=strings.end()&&found->text==u"*.R-net","Original file extension differs");
    auto imports=p.imports();require(imports.size()==23,"Import DLL count differs");
    auto ftd=std::find_if(imports.begin(),imports.end(),[](auto&i){return i.dll=="FTD2XX.dll";});
    require(ftd!=imports.end()&&ftd->symbols.size()==14,"FTD2XX ordinal import count differs");
    require(std::find(ftd->symbols.begin(),ftd->symbols.end(),"#75")!=ftd->symbols.end(),"FTD2XX ordinal 75 missing");
    auto m=std::find_if(resources.begin(),resources.end(),[](auto&r){return r.type=="4"&&r.name=="675";});
    require(m!=resources.end(),"OEM menu missing");auto menu=PeImage::menu(m->bytes);
    require(menu.size()==6&&menu[0].text==u"&File"&&menu[2].text==u"&Controller","Main menu differs");
    std::function<bool(const std::vector<MenuItem>&,unsigned)> contains;
    contains=[&](auto& entries,unsigned id){for(auto&e:entries)if(e.id==id||contains(e.children,id))return true;return false;};
    require(contains(menu,32803)&&contains(menu,32804)&&contains(menu,57665),"Recovered command IDs differ");
    std::size_t rejected=0;
    for(Bytes bad:{Bytes{},Bytes{0x4d,0x5a},Bytes(100,0)}){try{PeImage invalid(bad);}catch(const std::exception&){++rejected;}}
    require(rejected==3,"Malformed inputs accepted");
    try{PeImage::menu(Bytes{0,0,0,0,16,0});throw std::logic_error("Truncated menu accepted");}catch(const std::runtime_error&){}
    require(utf8(u"Gr\u00fc\u00dfe \U0001f680")=="Gr\xc3\xbc\xc3\x9f" "e \xf0\x9f\x9a\x80","UTF conversion failed");
    std::cout<<"PASS: PE32 entry, 585 resources, 1454 localized strings, 23 DLL imports, OEM menu IDs, malformed input, UTF-16\n";
    return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
