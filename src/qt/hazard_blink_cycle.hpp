#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
namespace rnet::qt {
class HazardBlinkCycleTest;
// Explicitly started lighting experiment; never a drive or parameter command.
class HazardBlinkCycle final : public QObject {
  friend class HazardBlinkCycleTest;
  QTimer interval_,endPulse_,watchdog_;
  QString interface_,error_;
  bool active_=false,pending_=false;
  int fd_=-1,index_=0,pulseMs_=60000;
  std::function<bool(bool)> sender_;
  bool transmit(bool first);
  void closeSocket();
  void pulse();
  void finishPulse();
  void fail(const QString &message);
  void changed();
public:
  explicit HazardBlinkCycle(QObject *parent=nullptr,std::function<bool(bool)> sender={});
  ~HazardBlinkCycle() override;
  bool configure(int intervalMs,int durationMs);
  int intervalMs()const{return interval_.interval();}
  int durationMs()const{return pulseMs_;}
  void start(const QString &interface);
  void stop();
  void quiesce(){if(pending_)finishPulse();}
  bool active()const{return active_;}
  bool pending()const{return pending_;}
  QString interface()const{return interface_;}
  QString error()const{return error_;}
  std::function<bool()> canPulse;
  std::function<void()> onChanged;
  std::function<void(const QString &)> onEvent;
};
}
