#include "can_observer.hpp"
#include <QCoreApplication>
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QSocketNotifier>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif
namespace attendant {
CanObserver::~CanObserver(){close();}
void CanObserver::close(){
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if(notifier_){notifier_->setEnabled(false);notifier_->deleteLater();notifier_=nullptr;}
    if(fd_>=0)::close(fd_);
#endif
    fd_=-1;emit changed();
}
bool CanObserver::open(const QString &name){
    close();receivedData_=false;count_=0;error_.clear();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    auto bytes=name.toLocal8Bit();
    if(bytes.isEmpty()||bytes.size()>=IFNAMSIZ||bytes.contains('\0')){error_=QCoreApplication::translate("RNet","Invalid CAN interface name.");return false;}
    fd_=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC,CAN_RAW);
    if(fd_<0){error_=QString::fromLocal8Bit(std::strerror(errno));return false;}
    ifreq request{};std::memcpy(request.ifr_name,bytes.constData(),bytes.size());
    if(::ioctl(fd_,SIOCGIFFLAGS,&request)<0||!(request.ifr_flags&IFF_UP)){
        error_=QCoreApplication::translate("RNet","CAN interface is missing or down.");close();return false;
    }
    if(::ioctl(fd_,SIOCGIFINDEX,&request)<0){error_=QString::fromLocal8Bit(std::strerror(errno));close();return false;}
    interfaceIndex_=request.ifr_ifindex;
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;
    if(::bind(fd_,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0){error_=QString::fromLocal8Bit(std::strerror(errno));close();return false;}
    notifier_=new QSocketNotifier(fd_,QSocketNotifier::Read,this);
    connect(notifier_,&QSocketNotifier::activated,this,[this]{read();});emit changed();return true;
#else
    Q_UNUSED(name);error_=QCoreApplication::translate("RNet","CAN observation is provided by the Linux host.");return false;
#endif
}
bool CanObserver::checkInterface(){
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if(fd_<0)return false;
    ifreq request{};request.ifr_ifindex=interfaceIndex_;
    if(::ioctl(fd_,SIOCGIFNAME,&request)<0 ||
       ::ioctl(fd_,SIOCGIFFLAGS,&request)<0 || !(request.ifr_flags&IFF_UP)) {
        error_=QCoreApplication::translate("RNet","CAN interface is missing or down.");
        close();
        return false;
    }
    return true;
#else
    return false;
#endif
}
void CanObserver::read(){
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    for(int i=0;i<64;++i){
        can_frame frame{};
        iovec buffer{&frame, sizeof(frame)};
        msghdr message{};
        message.msg_iov = &buffer;
        message.msg_iovlen = 1;
        const auto n = ::recvmsg(fd_, &message, 0);
        if(n==ssize_t(sizeof(frame))){
            ++count_;
            if ((message.msg_flags & MSG_DONTROUTE) || (frame.can_id & CAN_ERR_FLAG)) continue;
            if (frame.can_id == (CAN_RTR_FLAG | 0x002u)) emit powerOffRequested();
            if (!receivedData_ && !(frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG))) {
                receivedData_ = true;
                emit firstDataFrame();
            }
            if (!(frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) && !(message.msg_flags & MSG_DONTROUTE))
                emit dataFrame(frame.can_id & CAN_EFF_MASK, QByteArray(reinterpret_cast<const char *>(frame.data), qMin<int>(frame.can_dlc, 8)));
            if(fd_<0)return;
            if (!(frame.can_id & CAN_ERR_FLAG) && !(message.msg_flags & MSG_DONTROUTE))
                emit protocolFrame(frame.can_id & CAN_EFF_MASK, QByteArray(reinterpret_cast<const char *>(frame.data), (frame.can_id & CAN_RTR_FLAG) ? 0 : qMin<int>(frame.can_dlc, 8)));
            if(fd_<0)return;
            continue;
        }
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
        if(n<0&&errno==EINTR)continue;
        error_=QCoreApplication::translate("RNet","CAN receive failed.");close();break;
    }
    emit changed();
#endif
}
}
