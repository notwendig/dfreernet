// Private diagnostic: only the original's C8/C9 identification reads.
// No announce, authentication, heartbeat, session lock, power or write command.
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
namespace {
struct Query {
    int fd=-1;
    std::array<unsigned char,8> heartbeat{};bool have_heartbeat=false;
    ~Query(){if(fd>=0)close(fd);}
    bool read(unsigned node,unsigned odi,unsigned& value){
        if(node>=15||(odi!=0xc8&&odi!=0xc9))throw std::runtime_error("Identification-only query rejected");
        can_frame request{};request.can_id=0x78f;request.can_dlc=8;request.data[0]=0x40|node;request.data[1]=odi;
        if(write(fd,&request,sizeof(request))!=sizeof(request))throw std::runtime_error("CAN identification request failed");
        std::printf("TX 78F#%02XC%X000000000000\n",0x40|node,odi&15);std::fflush(stdout);
        auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(300);
        while(std::chrono::steady_clock::now()<end){
            pollfd wait{fd,POLLIN,0};int ready=poll(&wait,1,20);
            if(ready<0||wait.revents&(POLLERR|POLLHUP|POLLNVAL))throw std::runtime_error("CAN receive failed");
            if(!ready)continue;
            can_frame reply{};if(::read(fd,&reply,sizeof(reply))!=sizeof(reply))continue;
            if(reply.can_id==0xe&&reply.can_dlc==8){std::memcpy(heartbeat.data(),reply.data,8);have_heartbeat=true;}
            if(reply.can_id!=0x790+node||reply.can_dlc!=8||reply.data[1]!=odi||reply.data[2]||reply.data[3])continue;
            if(reply.data[0]!=0x2f){std::printf("NODE %u ODI %02X REJECTED TC=%02X\n",node,odi,reply.data[0]);return false;}
            value=0;for(unsigned i=0;i<4;++i)value|=unsigned(reply.data[4+i])<<(8*i);
            std::printf("RX %03X#",reply.can_id);for(auto byte:reply.data)std::printf("%02X",byte);std::printf("\n");return true;
        }
        std::printf("NODE %u ODI %02X NO_REPLY\n",node,odi);return false;
    }
};
}
int main(int argc,char** argv){try{
    if(argc!=2||std::strlen(argv[1])>=IFNAMSIZ)throw std::runtime_error("Usage: rnet_serial_query INTERFACE");
    Query query;query.fd=socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK,CAN_RAW);if(query.fd<0)throw std::runtime_error("Cannot open SocketCAN");
    ifreq request{};std::strcpy(request.ifr_name,argv[1]);if(ioctl(query.fd,SIOCGIFINDEX,&request)<0)throw std::runtime_error("Unknown CAN interface");
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;
    if(bind(query.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)))throw std::runtime_error("Cannot bind CAN interface");
    unsigned found=0;
    for(unsigned node=0;node<15;++node){
        unsigned low=0,high=0;
        if(query.read(node,0xc8,low)){
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if(query.read(node,0xc9,high)){std::printf("DIME node=%u raw=%08X%08X\n",node,high,low);++found;}
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if(query.have_heartbeat){std::printf("JSM_HEARTBEAT raw=");for(auto byte:query.heartbeat)std::printf("%02X",byte);std::printf("\n");}
    std::printf("COMPLETE identification_pairs=%u\n",found);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAILED: %s\n",error.what());return 1;}}
