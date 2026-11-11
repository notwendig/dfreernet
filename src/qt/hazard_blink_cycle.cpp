#include "hazard_blink_cycle.hpp"
#include <QCoreApplication>
#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace rnet::qt {
HazardBlinkCycle::HazardBlinkCycle(QObject *parent,std::function<bool(bool)> sender):QObject(parent),sender_(std::move(sender)){
  interval_.setInterval(5*60*1000);interval_.setTimerType(Qt::PreciseTimer);endPulse_.setSingleShot(true);endPulse_.setTimerType(Qt::PreciseTimer);
  retry_.setInterval(100);
  connect(&retry_,&QTimer::timeout,this,[this]{pulse();});
  if(!sender_)sender_=[this](bool first){return transmit(first);};
  connect(&interval_,&QTimer::timeout,this,[this]{pulse();});connect(&endPulse_,&QTimer::timeout,this,[this]{finishPulse();});
}
HazardBlinkCycle::~HazardBlinkCycle(){onChanged={};onEvent={};stop();}
void HazardBlinkCycle::changed(){if(onChanged)onChanged();}
void HazardBlinkCycle::closeSocket(){if(fd_>=0)::close(fd_);fd_=-1;index_=0;}
bool HazardBlinkCycle::transmit(bool first){
  if(first){
    closeSocket();const auto name=interface_.toLatin1();if(name.isEmpty()||name.size()>=IFNAMSIZ)return false;
    fd_=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK|SOCK_CLOEXEC,CAN_RAW);if(fd_<0)return false;
    ifreq request{};std::memcpy(request.ifr_name,name.constData(),name.size());
    if(::ioctl(fd_,SIOCGIFINDEX,&request)<0){closeSocket();return false;}index_=request.ifr_ifindex;
    if(::ioctl(fd_,SIOCGIFFLAGS,&request)<0||!(request.ifr_flags&IFF_UP)){closeSocket();return false;}
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=index_;
    if(::bind(fd_,reinterpret_cast<sockaddr*>(&address),sizeof(address))){closeSocket();return false;}
  }else if(fd_<0||int(::if_nametoindex(interface_.toLatin1().constData()))!=index_){closeSocket();return false;}
  // Verified one-shot on the user's stationary T5 VS: this frame toggles hazards.
  can_frame frame{};frame.can_id=CAN_EFF_FLAG|0x0c000403;frame.can_dlc=0;
  const bool sent=::send(fd_,&frame,sizeof(frame),MSG_DONTWAIT|MSG_NOSIGNAL)==sizeof(frame);
  if(!first||!sent)closeSocket();
  return sent;
}
bool HazardBlinkCycle::configure(int intervalMs,int durationMs){
  if(intervalMs<1||durationMs<1||durationMs>=intervalMs)return false;
  const bool intervalChanged=interval_.interval()!=intervalMs;
  if(pending_&&pulseMs_!=durationMs)finishPulse();
  if(intervalChanged)interval_.setInterval(intervalMs);
  pulseMs_=durationMs;
  if(active_&&intervalChanged)interval_.start();
  changed();return true;
}
void HazardBlinkCycle::start(const QString &interface){stop();interface_=interface;error_.clear();active_=true;interval_.start();if(onEvent)onEvent(QString("BLINK_TEST_STARTED interval_ms=%1 pulse_ms=%2 first=immediate").arg(interval_.interval()).arg(pulseMs_));changed();pulse();}
void HazardBlinkCycle::stop(){const bool wasActive=active_||pending_;active_=false;waiting_=false;retry_.stop();interval_.stop();if(pending_)finishPulse();endPulse_.stop();closeSocket();if(wasActive&&onEvent)onEvent("BLINK_TEST_STOPPED");changed();}
void HazardBlinkCycle::fail(const QString &message){if(onEvent)onEvent("BLINK_TEST_FAILED "+message);error_=message;active_=false;waiting_=false;retry_.stop();interval_.stop();endPulse_.stop();closeSocket();changed();}
void HazardBlinkCycle::pulse(){
  if(!active_||pending_)return;
  if(!canPulse||!canPulse()){
    if(!waiting_){waiting_=true;if(onEvent)onEvent("BLINK_TEST_WAITING no_heartbeat_or_controller_busy");changed();}
    if(!retry_.isActive())retry_.start();
    return;
  }
  waiting_=false;retry_.stop();
  if(!sender_(true)){fail(QCoreApplication::translate("RNet","Blink test stopped: the first light command could not be sent."));return;}
  pending_=true;interval_.start();endPulse_.start(pulseMs_);if(onEvent)onEvent("BLINK_TEST_FIRST_TOGGLE");changed();
}
void HazardBlinkCycle::finishPulse(){
  if(!pending_)return;
  endPulse_.stop();pending_=false;
  if(!sender_(false)){fail(QCoreApplication::translate("RNet","Blink test stopped: the second light command could not be sent. Check the hazard lights."));return;}
  if(onEvent)onEvent("BLINK_TEST_SECOND_TOGGLE");
  changed();
}
}
