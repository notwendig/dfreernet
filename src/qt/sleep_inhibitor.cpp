#include "sleep_inhibitor.hpp"
#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>
#include <fcntl.h>
namespace rnet::qt {
namespace {
void releaseIdle(const QDBusConnection &bus,quint32 cookie){
  auto call=QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver","/org/freedesktop/ScreenSaver","org.freedesktop.ScreenSaver","UnInhibit");call<<cookie;bus.asyncCall(call,1500);
}
}
SleepInhibitor::SleepInhibitor(QObject *parent,QDBusConnection system,QDBusConnection session):QObject(parent),system_(system),session_(session){
  retry_.setInterval(30000);connect(&retry_,&QTimer::timeout,this,[this]{acquire();});
}
SleepInhibitor::~SleepInhibitor(){onChanged={};configure(false);}
void SleepInhibitor::changed(){if(onChanged)onChanged();}
void SleepInhibitor::configure(bool enabled){
  if(requested_!=enabled){requested_=enabled;++generation_;pendingSleep_=pendingIdle_=false;}
  if(enabled){retry_.start();acquire();}
  else{retry_.stop();sleepFd_=QDBusUnixFileDescriptor();if(idleHeld_)releaseIdle(session_,idleCookie_);idleHeld_=false;changed();}
}
void SleepInhibitor::acquire(){
  if(!requested_)return;
  if(!sleepBlocked()&&!pendingSleep_)acquireSleep("sleep:idle");
  if(!idleHeld_&&!pendingIdle_)acquireIdle();
}
void SleepInhibitor::acquireSleep(const QString &what){
  pendingSleep_=true;changed();const auto generation=generation_;QPointer<SleepInhibitor> guard(this);
  auto call=QDBusMessage::createMethodCall("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","Inhibit");
  call<<what<<QString("dfreernet")<<QCoreApplication::translate("RNet","R-net configuration work in progress")<<QString("block");
  auto *watcher=new QDBusPendingCallWatcher(system_.asyncCall(call,1500),QCoreApplication::instance());
  connect(watcher,&QDBusPendingCallWatcher::finished,QCoreApplication::instance(),[guard,generation,what](QDBusPendingCallWatcher *done){
    QDBusPendingReply<QDBusUnixFileDescriptor> reply=*done;done->deleteLater();
    if(!guard||!guard->requested_||guard->generation_!=generation)return; // Late FDs close automatically.
    guard->pendingSleep_=false;
    if(!reply.isError()&&reply.value().isValid()){
      guard->sleepFd_=reply.value();auto fd=guard->sleepFd_.fileDescriptor();auto flags=::fcntl(fd,F_GETFD);if(flags>=0)::fcntl(fd,F_SETFD,flags|FD_CLOEXEC);
    }else if(what=="sleep:idle"){guard->acquireSleep("sleep");return;}
    guard->changed();
  });
}
void SleepInhibitor::acquireIdle(){
  pendingIdle_=true;changed();const auto generation=generation_;QPointer<SleepInhibitor> guard(this);const auto bus=session_;
  auto call=QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver","/org/freedesktop/ScreenSaver","org.freedesktop.ScreenSaver","Inhibit");
  call<<QString("dfreernet")<<QCoreApplication::translate("RNet","R-net configuration work in progress");
  auto *watcher=new QDBusPendingCallWatcher(session_.asyncCall(call,1500),QCoreApplication::instance());
  connect(watcher,&QDBusPendingCallWatcher::finished,QCoreApplication::instance(),[guard,generation,bus](QDBusPendingCallWatcher *done){
    QDBusPendingReply<quint32> reply=*done;done->deleteLater();
    if(!guard||!guard->requested_||guard->generation_!=generation){if(!reply.isError())releaseIdle(bus,reply.value());return;}
    guard->pendingIdle_=false;
    if(!reply.isError()){guard->idleCookie_=reply.value();guard->idleHeld_=true;}
    guard->changed();
  });
}
QString SleepInhibitor::statusText()const{
  auto tr=[](const char*s){return QCoreApplication::translate("RNet",s);};
  if(!requested_)return tr("Automatic sleep prevention disabled");
  if(sleepBlocked()&&idleHeld_)return tr("Automatic system sleep and screen blanking inhibited");
  if(sleepBlocked())return tr("System sleep inhibited; desktop idle inhibition unavailable");
  if(idleHeld_)return tr("Desktop idle inhibited; system sleep inhibition unavailable");
  if(pendingSleep_||pendingIdle_)return tr("Requesting sleep prevention…");
  return tr("Sleep prevention unavailable in this session");
}
}
