#include "can_heartbeat.hpp"
#include <QtWidgets>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace rnet::qt {
CanHeartbeatIndicator::CanHeartbeatIndicator(QWidget *parent):QWidget(parent),timer_(new QTimer(this)){
  setObjectName("canHeartbeat");setFixedSize(34,26);animation_.start();
  connect(timer_,&QTimer::timeout,this,[this]{
    if(enabled_&&fd_>=0&&(!probe_.isValid()||probe_.elapsed()>250))checkSocketState();
    if(enabled_&&fd_<0&&(!retry_.isValid()||retry_.elapsed()>1000))openSocket();
    refresh();
  });
}
CanHeartbeatIndicator::~CanHeartbeatIndicator(){closeSocket();}
void CanHeartbeatIndicator::closeSocket(){delete notifier_;notifier_=nullptr;if(fd_>=0)::close(fd_);fd_=-1;boundIndex_=0;lastFrame_.invalidate();}
void CanHeartbeatIndicator::configure(const QString &interface,bool enabled,int timeoutMs){
  closeSocket();interface_=interface;enabled_=enabled;timeoutMs_=qBound(500,timeoutMs,10000);
  if(enabled_){openSocket();timer_->start(40);}else timer_->stop();refresh();
}
void CanHeartbeatIndicator::openSocket(){
  retry_.restart();interfaceDown_=false;if(interface_.isEmpty()||interface_.size()>=IFNAMSIZ)return;
  fd_=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC,CAN_RAW);if(fd_<0)return;
  ifreq request{};auto name=interface_.toLatin1();std::memcpy(request.ifr_name,name.constData(),name.size());
  if(::ioctl(fd_,SIOCGIFINDEX,&request)<0){closeSocket();return;}
  const int index=request.ifr_ifindex;
  if(::ioctl(fd_,SIOCGIFFLAGS,&request)<0){closeSocket();return;}
  if(!(request.ifr_flags&IFF_UP)){interfaceDown_=true;closeSocket();return;}
  // Observed JSM serial heartbeat 0x00E. Match standard and extended frames;
  // remote/error frames are never treated as a heartbeat.
  can_filter filter{0x00e,CAN_EFF_MASK|CAN_RTR_FLAG};
  if(::setsockopt(fd_,SOL_CAN_RAW,CAN_RAW_FILTER,&filter,sizeof(filter))){closeSocket();return;}
  sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=index;
  if(::bind(fd_,reinterpret_cast<sockaddr*>(&address),sizeof(address))){closeSocket();return;}
  boundIndex_=index;probe_.restart();
  notifier_=new QSocketNotifier(fd_,QSocketNotifier::Read,this);
  connect(notifier_,&QSocketNotifier::activated,this,[this]{receive();});
}
void CanHeartbeatIndicator::checkSocketState(){
  probe_.restart();if(fd_<0)return;
  ifreq request{};const auto name=interface_.toLatin1();std::memcpy(request.ifr_name,name.constData(),name.size());
  if(::ioctl(fd_,SIOCGIFINDEX,&request)<0){interfaceDown_=false;closeSocket();retry_.restart();return;}
  if(request.ifr_ifindex!=boundIndex_){closeSocket();openSocket();return;}
  if(::ioctl(fd_,SIOCGIFFLAGS,&request)<0){interfaceDown_=false;closeSocket();retry_.restart();return;}
  if(!(request.ifr_flags&IFF_UP)){interfaceDown_=true;closeSocket();retry_.restart();}
}
void CanHeartbeatIndicator::receive(){
  for(int count=0;count<64;++count){
    can_frame frame{};auto length=::read(fd_,&frame,sizeof(frame));
    if(length<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
    if(length!=sizeof(frame)){closeSocket();break;}
    if(!(frame.can_id&(CAN_ERR_FLAG|CAN_RTR_FLAG))&&(frame.can_id&CAN_EFF_MASK)==0x00e&&frame.can_dlc==8)lastFrame_.restart();
  }
  refresh();
}
bool CanHeartbeatIndicator::receiving()const{return enabled_&&lastFrame_.isValid()&&lastFrame_.elapsed()<timeoutMs_;}
void CanHeartbeatIndicator::refresh(){
  QString state;
  if(!enabled_)state=QCoreApplication::translate("RNet","CAN heartbeat disabled");
  else if(interfaceDown_)state=QCoreApplication::translate("RNet","CAN %1: interface is down").arg(interface_);
  else if(fd_<0)state=QCoreApplication::translate("RNet","CAN %1: interface unavailable").arg(interface_);
  else if(receiving())state=QCoreApplication::translate("RNet","CAN %1: heartbeat received (receive only)").arg(interface_);
  else state=QCoreApplication::translate("RNet","CAN %1: no heartbeat").arg(interface_);
  if(state!=status_){status_=state;setToolTip(state);setAccessibleName(state);}
  update();
}
void CanHeartbeatIndicator::changeEvent(QEvent *event){QWidget::changeEvent(event);if(event->type()==QEvent::LanguageChange)refresh();}
void CanHeartbeatIndicator::paintEvent(QPaintEvent*){
  QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
  bool active=receiving();double phase=std::fmod(animation_.elapsed()/1000.,1.);
  double beat=std::exp(-std::pow((phase-.17)/.07,2))+.65*std::exp(-std::pow((phase-.37)/.08,2));
  double scale=active?.84+.18*beat:.84;
  painter.translate(width()/2.,height()/2.);painter.scale(scale,scale);
  QPainterPath heart;heart.moveTo(0,10);heart.cubicTo(-3,7,-12,1,-10,-5);heart.cubicTo(-8,-11,-2,-10,0,-6);heart.cubicTo(2,-10,8,-11,10,-5);heart.cubicTo(12,1,3,7,0,10);
  if(active){painter.setPen(QPen(QColor(220,53,75,int(35+35*beat)),3));painter.setBrush(Qt::NoBrush);painter.drawPath(heart);}
  painter.setPen(Qt::NoPen);painter.setBrush(active?QColor("#D83C55"):palette().color(QPalette::Disabled,QPalette::Text));painter.drawPath(heart);
}
}
