#include "can_heartbeat.hpp"
#include <QtWidgets>
#include <cmath>
#include <linux/can.h>
namespace rnet::qt {
CanHeartbeatIndicator::CanHeartbeatIndicator(QWidget *parent):QWidget(parent),timer_(new QTimer(this)){
  setObjectName("canHeartbeat");setFixedSize(34,26);animation_.start();
  connect(timer_,&QTimer::timeout,this,[this]{refresh();});
}
void CanHeartbeatIndicator::configure(const QString &interface,bool enabled,int timeoutMs){
  if(interface_!=interface)lastFrame_.invalidate();
  interface_=interface;enabled_=enabled;timeoutMs_=qBound(500,timeoutMs,10000);
  if(enabled_)timer_->start(40);else timer_->stop();refresh();
}
void CanHeartbeatIndicator::setSocketState(bool available,bool interfaceDown){
  if(socketAvailable_!=available||interfaceDown_!=interfaceDown)lastFrame_.invalidate();
  socketAvailable_=available;interfaceDown_=interfaceDown;refresh();
}
void CanHeartbeatIndicator::observe(const can_frame &frame){
  if(!enabled_||!socketAvailable_)return;
  // The 0x00E serial announcement does not establish cyclic traffic.
  if(frame.can_id==(CAN_EFF_FLAG|0x03c30f0fU)&&frame.can_dlc==7){
    bool complete=true;for(int i=0;i<7;++i)complete=complete&&frame.data[i]==0x87;
    if(complete)lastFrame_.restart();
  }
  refresh();
}
bool CanHeartbeatIndicator::receiving()const{return enabled_&&socketAvailable_&&lastFrame_.isValid()&&lastFrame_.elapsed()<timeoutMs_;}
void CanHeartbeatIndicator::refresh(){
  QString state;
  if(!enabled_)state=QCoreApplication::translate("RNet","CAN heartbeat disabled");
  else if(interfaceDown_)state=QCoreApplication::translate("RNet","CAN %1: interface is down").arg(interface_);
  else if(!socketAvailable_)state=QCoreApplication::translate("RNet","CAN %1: interface unavailable").arg(interface_);
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
