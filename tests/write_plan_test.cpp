#include "values.hpp"
#include "runtime_resources.hpp"
#include <fstream>
#include <iostream>
using namespace rnet;
void save(const std::filesystem::path &p,const Bytes &b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("fixture write failed");}
int main(int argc,char **argv){try{
 if(argc!=4&&argc!=5)throw std::runtime_error("fixture installation output required");
 auto output=std::filesystem::path(argv[3]);
 auto catalog=bundled_catalog();
 auto before=RnetDocument::read(read_file(argv[1])),after=before;
 std::vector<std::string>warnings;auto values=read_values(before,catalog,&warnings);
 bool speed=false,key=false,enumeration=false;
 for(const auto &v:values){
   if(argc==5&&v.parameter==810&&!enumeration){edit_value(after,catalog,v,u"0",AccessLevel::OEM);enumeration=true;}
   if(v.parameter==68){edit_value(after,catalog,v,u"1",AccessLevel::OEM);key=true;}
   if(v.parameter==312&&v.profile==0&&!speed){edit_value(after,catalog,v,v.number==86?u"85":u"86",AccessLevel::OEM);speed=true;}
 }
 if(!speed||!key)throw std::runtime_error("missing parameters");
 auto plan=controller_write_plan(before,after,catalog,AccessLevel::OEM);
 if(plan.size()!=2 || (argc==5&&!enumeration))throw std::runtime_error("expected two-file plan with requested values");
 bool denied=false;try{controller_write_plan(before,after,catalog,AccessLevel::Therapist);}catch(const std::runtime_error&){denied=true;}
 if(!denied)throw std::runtime_error("access bypass");
 auto corrupt=after;corrupt.metadata.push_back(0);denied=false;
 try{controller_write_plan(before,corrupt,catalog,AccessLevel::Factory);}catch(const std::runtime_error&){denied=true;}
 if(!denied)throw std::runtime_error("metadata bypass");
 std::filesystem::create_directories(output);save(output/"before.R-net",before.write());save(output/"desired.R-net",after.write());
 std::ofstream manifest(output/"files.csv");manifest<<"id,version\n";
 for(auto i:plan){auto name=std::to_string(before.files[i].id);save(output/(name+"-before.bin"),before.files[i].data);save(output/(name+"-after.bin"),after.files[i].data);manifest<<name<<','<<before.files[i].version<<'\n';}
 std::cout<<"PASS: replayed multi-file write plan, access and metadata guards; fixtures emitted\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
