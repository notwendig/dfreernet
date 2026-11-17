#include "hazard_blink_cycle.hpp"
#include <QCoreApplication>
namespace rnet::qt {
HazardBlinkCycle::HazardBlinkCycle(QObject *parent,std::function<bool(bool)> sender):QObject(parent),sender_(std::move(sender)){
  interval_.setInterval(5*60*1000);interval_.setTimerType(Qt::PreciseTimer);endPulse_.setSingleShot(true);endPulse_.setTimerType(Qt::PreciseTimer);
  watchdog_.setInterval(100);
  connect(&watchdog_,&QTimer::timeout,this,[this]{
    if(!active_|| (canPulse&&canPulse()))return;
    if(pending_)finishPulse();
    if(active_)fail(QCoreApplication::translate("RNet","Light cycle stopped: CAN heartbeat or controller access unavailable. Check the hazard lights."));
  });
  connect(&interval_,&QTimer::timeout,this,[this]{pulse();});connect(&endPulse_,&QTimer::timeout,this,[this]{finishPulse();});
}
HazardBlinkCycle::~HazardBlinkCycle(){onChanged={};onEvent={};stop();}
void HazardBlinkCycle::changed(){if(onChanged)onChanged();}
bool HazardBlinkCycle::configure(int intervalMs,int durationMs){
  if(intervalMs<1||durationMs<1||durationMs>=intervalMs)return false;
  const bool intervalChanged=interval_.interval()!=intervalMs;
  if(pending_&&pulseMs_!=durationMs)finishPulse();
  if(intervalChanged)interval_.setInterval(intervalMs);
  pulseMs_=durationMs;
  if(active_&&intervalChanged)interval_.start();
  changed();return true;
}
void HazardBlinkCycle::start(const QString &interface){stop();interface_=interface;error_.clear();active_=true;interval_.start();watchdog_.start();if(onEvent)onEvent(QString("BLINK_TEST_STARTED interval_ms=%1 pulse_ms=%2 first=immediate").arg(interval_.interval()).arg(pulseMs_));changed();pulse();}
void HazardBlinkCycle::stop(){const bool wasActive=active_||pending_;active_=false;watchdog_.stop();interval_.stop();if(pending_)finishPulse();endPulse_.stop();if(wasActive&&onEvent)onEvent("BLINK_TEST_STOPPED");changed();}
void HazardBlinkCycle::fail(const QString &message){if(onEvent)onEvent("BLINK_TEST_FAILED "+message);error_=message;active_=false;watchdog_.stop();interval_.stop();endPulse_.stop();changed();}
void HazardBlinkCycle::pulse(){
  if(!active_||pending_)return;
  if(!canPulse||!canPulse()){
    fail(QCoreApplication::translate("RNet","Light cycle stopped: CAN heartbeat or controller access unavailable. Check the hazard lights."));
    return;
  }
  if(!sender_||!sender_(true)){fail(QCoreApplication::translate("RNet","Blink test stopped: the first light command could not be sent."));return;}
  pending_=true;interval_.start();endPulse_.start(pulseMs_);if(onEvent)onEvent("BLINK_TEST_FIRST_TOGGLE");changed();
}
void HazardBlinkCycle::finishPulse(){
  if(!pending_)return;
  endPulse_.stop();pending_=false;
  if(!sender_||!sender_(false)){fail(QCoreApplication::translate("RNet","Blink test stopped: the second light command could not be sent. Check the hazard lights."));return;}
  if(onEvent)onEvent("BLINK_TEST_SECOND_TOGGLE");
  changed();
}
}
