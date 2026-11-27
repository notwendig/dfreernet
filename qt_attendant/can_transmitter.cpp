#include "can_transmitter.hpp"
#include <QCoreApplication>
#include <QRegularExpression>
#include <cstring>
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace attendant {
namespace {QString text(const char *s){return QCoreApplication::translate("RNet",s);}}
bool parse_transmit_frame(const QString &value, TransmitFrame &out){
    static const QRegularExpression syntax(QStringLiteral("^([0-9a-fA-F]{3}|[0-9a-fA-F]{8})#([0-9a-fA-F]{0,16}|R[0-8]?)$"));
    const auto match=syntax.match(value.trimmed());
    if(!match.hasMatch())return false;
    TransmitFrame frame;
    frame.extended=match.captured(1).size()==8;
    frame.id=match.captured(1).toUInt(nullptr,16);
    if(frame.id>(frame.extended?0x1fffffffu:0x7ffu))return false;
    const auto payload=match.captured(2);
    frame.remote=payload.startsWith('R');
    if(frame.remote)frame.length=payload.size()==2?payload.at(1).digitValue():0;
    else{
        if(payload.size()%2)return false;
        frame.data=QByteArray::fromHex(payload.toLatin1());
        frame.length=static_cast<unsigned char>(frame.data.size());
    }
    out=frame;return true;
}
QByteArray CanTransmitter::encode(const TransmitFrame &frame){
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if(frame.length>8||frame.id>(frame.extended?CAN_EFF_MASK:CAN_SFF_MASK)||
       (frame.remote?!frame.data.isEmpty():frame.data.size()!=frame.length))return {};
    can_frame packet{};
    packet.can_id=frame.id|(frame.extended?CAN_EFF_FLAG:0)|(frame.remote?CAN_RTR_FLAG:0);
    packet.can_dlc=frame.length;
    if(!frame.remote&&!frame.data.isEmpty())std::memcpy(packet.data,frame.data.constData(),frame.data.size());
    return QByteArray(reinterpret_cast<const char *>(&packet),sizeof(packet));
#else
    Q_UNUSED(frame);return {};
#endif
}
CanTransmitter::~CanTransmitter(){close();}
void CanTransmitter::close(){
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if(fd_>=0)::close(fd_);
    if(lease_>=0)::close(lease_);
#endif
    fd_=lease_=-1;emit changed();
}
bool CanTransmitter::open(const QString &interface){
    close();error_.clear();count_=0;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    const auto name=interface.toLocal8Bit();
    if(name.isEmpty()||name.size()>=IFNAMSIZ||!QRegularExpression("^[A-Za-z0-9_.-]+$").match(interface).hasMatch()){
        error_=text("Invalid CAN interface name.");emit changed();return false;
    }
    fd_=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC,CAN_RAW);
    if(fd_<0){error_=QString::fromLocal8Bit(std::strerror(errno));emit changed();return false;}
    // TX-only socket: do not accumulate unrelated incoming traffic.
    if(::setsockopt(fd_,SOL_CAN_RAW,CAN_RAW_FILTER,nullptr,0)<0){
        error_=QString::fromLocal8Bit(std::strerror(errno));close();return false;
    }
    ifreq request{};std::memcpy(request.ifr_name,name.constData(),name.size());
    if(::ioctl(fd_,SIOCGIFFLAGS,&request)<0||!(request.ifr_flags&IFF_UP)){
        error_=text("CAN interface is missing or down.");close();return false;
    }
    if(::ioctl(fd_,SIOCGIFINDEX,&request)<0){error_=QString::fromLocal8Bit(std::strerror(errno));close();return false;}
    const auto leaseName = QString("/tmp/rnet-socketcan-%1-%2.lock").arg(::geteuid()).arg(interface).toLocal8Bit();
    lease_ = ::open(leaseName.constData(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
    struct stat info{};
    if (lease_<0 || ::fstat(lease_,&info)!=0 || info.st_uid!=::geteuid() ||
        ::flock(lease_,LOCK_EX|LOCK_NB)!=0) {
        error_=text("Another R-net client owns the CAN interface");
        close();
        return false;
    }
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;
    if(::bind(fd_,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0){
        error_=QString::fromLocal8Bit(std::strerror(errno));close();return false;
    }
    emit changed();return true;
#else
    Q_UNUSED(interface);error_=text("Manual CAN transmission requires Linux.");emit changed();return false;
#endif
}
bool CanTransmitter::send(const QString &value){
    error_.clear();
    TransmitFrame frame;
    if(!parse_transmit_frame(value,frame)){error_=text("Invalid CAN frame. Use 123#0102 or 02000300#0000.");emit changed();return false;}
    if(fd_<0){error_=text("CAN transmitter is stopped.");emit changed();return false;}
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    const auto packet=encode(frame);
    // One bounded, nonblocking attempt; no retry queue or periodic transmission.
    const auto sent=::write(fd_,packet.constData(),packet.size());
    if(sent!=packet.size()){
        error_=sent<0?QString::fromLocal8Bit(std::strerror(errno)):text("Incomplete CAN transmission.");
        close();return false;
    }
    ++count_;emit changed();return true;
#else
    return false;
#endif
}
}
