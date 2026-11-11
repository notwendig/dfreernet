#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QTimer>
#include <functional>
namespace rnet::qt {
// Lifetime-scoped Linux/desktop inhibitors. No controller or CAN commands.
class SleepInhibitor final : public QObject {
  QDBusConnection system_,session_;
  QDBusUnixFileDescriptor sleepFd_;
  QTimer retry_;
  quint64 generation_=0;
  quint32 idleCookie_=0;
  bool requested_=false,pendingSleep_=false,pendingIdle_=false,idleHeld_=false;
  void acquire();
  void acquireSleep(const QString &what);
  void acquireIdle();
  void changed();
public:
  explicit SleepInhibitor(QObject *parent=nullptr,QDBusConnection system=QDBusConnection::systemBus(),QDBusConnection session=QDBusConnection::sessionBus());
  ~SleepInhibitor() override;
  void configure(bool enabled);
  bool requested()const{return requested_;}
  bool sleepBlocked()const{return sleepFd_.isValid();}
  bool idleBlocked()const{return idleHeld_;}
  QString statusText()const;
  std::function<void()> onChanged;
};
}
