#include <winsock2.h>
#include <windows.h>
#include "can_udp.hpp"
#include <array>
#include <fstream>
#include <iomanip>
namespace rnet {
namespace {
class UdpTransport final:public CanTransport {
    SOCKET socket_=INVALID_SOCKET;
    bool initialized_=false;
    bool receive_only_=false;
    std::ofstream log_;
    void record(const char* direction,const CanFrame& frame){
        log_<<direction<<' '<<std::hex<<std::uppercase<<frame.id<<'#';
        for(auto b:frame.data)log_<<std::setw(2)<<std::setfill('0')<<unsigned(b);
        log_<<'\n';log_.flush();
    }
    void packet(unsigned type,const CanFrame& frame){
        if(frame.data.size()>8||frame.id>0x1fffffff)throw std::runtime_error("Invalid CAN frame");
        std::array<char,20> bytes{};bytes[0]='R';bytes[1]='N';bytes[2]='C';bytes[3]='1';bytes[4]=char(type);bytes[5]=char(frame.data.size());
        for(unsigned i=0;i<4;++i)bytes[6+i]=char(frame.id>>(8*i));
        std::copy(frame.data.begin(),frame.data.end(),bytes.begin()+10);
        if(::send(socket_,bytes.data(),bytes.size(),0)!=int(bytes.size()))throw std::runtime_error("RNC1 send failed");
    }
public:
    explicit UdpTransport(const std::filesystem::path& path,bool receive_only=false):receive_only_(receive_only),log_(path){
        // This first backend is only enabled for namespace-isolated emulator
        // experiments. The runner proves the bridge uses private vcan.
        wchar_t enabled[8]{},prefix[32768]{};
        GetEnvironmentVariableW(L"RNET_ISOLATED_EMULATOR_TEST",enabled,8);
        GetEnvironmentVariableW(L"WINEPREFIX",prefix,32768);
        if(!receive_only_&&(std::wstring(enabled)!=L"1"||std::wstring(prefix).rfind(L"/tmp/rnet-direct-",0)!=0))throw std::runtime_error("Direct CAN backend requires the isolated emulator runner");
        if(!log_)throw std::runtime_error("Cannot create CAN evidence log");
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Winsock initialization failed");initialized_=true;
        socket_=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(socket_==INVALID_SOCKET){WSACleanup();initialized_=false;throw std::runtime_error("UDP socket creation failed");}
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(receive_only_?39010:39000);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(::connect(socket_,reinterpret_cast<sockaddr*>(&address),sizeof(address))){closesocket(socket_);WSACleanup();initialized_=false;throw std::runtime_error("Loopback proxy connection failed");}
        int buffer=1024*1024;setsockopt(socket_,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<const char*>(&buffer),sizeof(buffer));
        try{packet(1,{});}catch(...){closesocket(socket_);WSACleanup();initialized_=false;throw;}
    }
    ~UdpTransport()override{if(socket_!=INVALID_SOCKET)closesocket(socket_);if(initialized_)WSACleanup();}
    void send(const CanFrame& frame)override{
        if(receive_only_)throw std::runtime_error("Physical CAN monitor is receive-only");
        packet(2,frame);record("TX",frame);
    }
    bool receive(CanFrame& frame,unsigned timeout_ms)override{
        fd_set set;FD_ZERO(&set);FD_SET(socket_,&set);timeval timeout{long(timeout_ms/1000),long((timeout_ms%1000)*1000)};
        int ready=select(0,&set,nullptr,nullptr,&timeout);if(ready==SOCKET_ERROR)throw std::runtime_error("UDP select failed");if(!ready)return false;
        std::array<unsigned char,64> bytes{};int size=recv(socket_,reinterpret_cast<char*>(bytes.data()),bytes.size(),0);
        if(size==SOCKET_ERROR)throw std::runtime_error("UDP receive failed");
        if(size!=20||bytes[0]!='R'||bytes[1]!='N'||bytes[2]!='C'||bytes[3]!='1'||bytes[5]>8)throw std::runtime_error("Malformed RNC1 datagram");
        if(bytes[4]==4)return false;
        if(bytes[4]!=3)throw std::runtime_error("Unexpected RNC1 packet type");
        frame={};for(unsigned i=0;i<4;++i)frame.id|=std::uint32_t(bytes[6+i])<<(8*i);
        if(frame.id>0x1fffffff)throw std::runtime_error("Invalid RNC1 identifier");
        frame.data.assign(bytes.begin()+10,bytes.begin()+10+bytes[5]);record("RX",frame);return true;
    }
};
}
std::unique_ptr<CanTransport> open_can_emulator_transport(const std::filesystem::path& log){return std::make_unique<UdpTransport>(log);}
std::unique_ptr<CanTransport> open_can_monitor_transport(const std::filesystem::path& log){return std::make_unique<UdpTransport>(log,true);}
}
