#include "rnd_decoder.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
    if(argc!=4)throw std::runtime_error("Usage: rnet_decode_tool ORIGINAL_EXE ENCRYPTED_RND NEW_OUTPUT");
    if(std::filesystem::exists(argv[3]))throw std::runtime_error("Output exists; refusing overwrite");
    rnet::PeImage exe(rnet::read_file(argv[1]));rnet::RndDecoder decoder(exe);
    auto bytes=decoder.decode(rnet::read_file(argv[2]));std::ofstream f(argv[3],std::ios::binary);
    if(!f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()))throw std::runtime_error("Write failed");
    std::cout<<"Decoded "<<bytes.size()<<" bytes, schema "<<unsigned(bytes[0])<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
