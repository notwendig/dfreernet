// Native Linux receive-only SocketCAN -> loopback bridge. There is no path
// that writes a CAN frame, including for malformed or type-2 UDP requests.
#include <arpa/inet.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv){
    if(argc!=2||std::strlen(argv[1])>=IFNAMSIZ){std::fprintf(stderr,"Usage: rnet_can_rx_bridge INTERFACE (receive only, port 39010)\n");return 2;}
    int can=socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK,CAN_RAW),udp=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);
    if(can<0||udp<0){std::perror("socket");return 1;}
    ifreq request{};std::strcpy(request.ifr_name,argv[1]);
    if(ioctl(can,SIOCGIFINDEX,&request)<0){std::perror("interface");return 1;}
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;
    if(bind(can,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){std::perror("bind CAN");return 1;}
    sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);local.sin_port=htons(39010);
    if(bind(udp,reinterpret_cast<sockaddr*>(&local),sizeof(local))<0){std::perror("bind UDP");return 1;}
    std::printf("RECEIVE ONLY: %s -> 127.0.0.1:39010; no CAN transmit implementation\n",argv[1]);std::fflush(stdout);
    sockaddr_in peer{};bool have_peer=false;
    pollfd fds[2]{{can,POLLIN,0},{udp,POLLIN,0}};
    for(;;){
        int ready=poll(fds,2,-1);if(ready<0){if(errno==EINTR)continue;std::perror("poll");return 1;}
        if(fds[0].revents&(POLLERR|POLLHUP|POLLNVAL)){std::fprintf(stderr,"Receive bridge CAN socket failed\n");return 1;}
        if(fds[1].revents&(POLLHUP|POLLNVAL)){std::fprintf(stderr,"Receive bridge UDP socket failed\n");return 1;}
        if(fds[1].revents&POLLERR){
            // A ten-second viewer closes its UDP socket. Clear any resulting
            // ICMP error and await a new hello, rather than terminating the
            // receive bridge or repeatedly sending to the departed viewer.
            int error=0;socklen_t length=sizeof(error);getsockopt(udp,SOL_SOCKET,SO_ERROR,&error,&length);have_peer=false;
        }
        if(fds[1].revents&POLLIN){
            std::array<unsigned char,64> bytes{};sockaddr_in from{};socklen_t length=sizeof(from);
            int size=recvfrom(udp,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&from),&length);
            if(size==20&&std::memcmp(bytes.data(),"RNC1",4)==0&&bytes[4]==1){
                peer=from;have_peer=true;std::array<unsigned char,20> ack{};std::memcpy(ack.data(),"RNC1",4);ack[4]=4;
                sendto(udp,ack.data(),ack.size(),0,reinterpret_cast<sockaddr*>(&peer),sizeof(peer));
            }
            // Every other packet is discarded, including any CAN-TX request.
        }
        if(fds[0].revents&POLLIN){
            can_frame frame{};auto size=read(can,&frame,sizeof(frame));
            if(size!=static_cast<ssize_t>(sizeof(frame))||!have_peer||(frame.can_id&(CAN_ERR_FLAG|CAN_RTR_FLAG)))continue;
            auto id=frame.can_id&((frame.can_id&CAN_EFF_FLAG)?CAN_EFF_MASK:CAN_SFF_MASK);
            std::array<unsigned char,20> bytes{};std::memcpy(bytes.data(),"RNC1",4);bytes[4]=3;bytes[5]=frame.can_dlc>8?8:frame.can_dlc;
            for(unsigned i=0;i<4;++i)bytes[6+i]=static_cast<unsigned char>(id>>(8*i));
            std::memcpy(bytes.data()+10,frame.data,bytes[5]);
            sendto(udp,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&peer),sizeof(peer));
        }
    }
}
