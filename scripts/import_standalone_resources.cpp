// One-time developer import. Not part of the application build or runtime.
#include "database.hpp"
#include "rnd_decoder.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace rnet;
static void save(const std::filesystem::path &p,const Bytes &b){if(std::filesystem::exists(p))throw std::runtime_error("Refuse replacing imported data");std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("Cannot export resource");}
static std::string json(const std::u16string &s){std::string r="\"";for(unsigned char c:utf8(s)){if(c=='"'||c=='\\'){r+='\\';r+=char(c);}else if(c<32){const char*h="0123456789abcdef";r+="\\u00";r+=h[c>>4];r+=h[c&15];}else r+=char(c);}return r+'"';}
static std::string menus(const std::vector<MenuItem>&items){std::string s="[";for(const auto&i:items){if(s.size()>1)s+=',';s+="{\"id\":"+std::to_string(i.id)+",\"flags\":"+std::to_string(i.flags)+",\"text\":"+json(i.text)+",\"children\":"+menus(i.children)+"}";}return s+"]";}
int main(int argc,char**argv){try{if(argc!=4)throw std::runtime_error("Usage: importer ORIGINAL_DIRECTORY TEMPLATE OUTPUT_DIRECTORY");std::filesystem::path source=argv[1],out=argv[3];std::filesystem::create_directories(out);PeImage exe(read_file(source/"RNet Programmer6 OEM.exe"));auto plain=RndDecoder(exe).decode(read_file(source/"Generic_V33_1_1245.rnd"));auto catalog=ParameterCatalog::read(plain);save(out/"catalog.raw",plain);save(out/"blowfish-pi.bin",exe.read_va(0x6c2e70,18*4+1024*4));save(out/"document-template.raw",read_file(argv[2]));for(const auto&r:exe.resources())if(r.type=="4"&&r.name=="675"){auto s=menus(PeImage::menu(r.bytes));save(out/"menu.json",Bytes(s.begin(),s.end()));break;}std::string strings="[";for(const auto&t:exe.strings()){if(strings.size()>1)strings+=',';strings+="{\"id\":"+std::to_string(t.id)+",\"text\":"+json(t.text)+"}";}strings+=']';save(out/"texts.json",Bytes(strings.begin(),strings.end()));std::cout<<catalog.parameters.size()<<" parameters, "<<catalog.storage_maps.size()<<" storage maps, "<<catalog.editors.size()<<" editors\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
