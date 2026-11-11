#pragma once
#include <QWidget>
#include <QElapsedTimer>
class QSocketNotifier;class QTimer;
namespace rnet::qt {
// Passive observer: no send/write, repository ownership or controller commands.
class CanHeartbeatIndicator final : public QWidget {
  int fd_=-1,boundIndex_=0;
  bool interfaceDown_=false;
  QSocketNotifier *notifier_=nullptr;
  QTimer *timer_;
  QString interface_,status_;
  bool enabled_=true;
  int timeoutMs_=1500;
  QElapsedTimer lastFrame_,animation_,retry_,probe_;
  void openSocket();
  void closeSocket();
  void receive();
  void checkSocketState();
  void refresh();
public:
  explicit CanHeartbeatIndicator(QWidget *parent=nullptr);
  ~CanHeartbeatIndicator() override;
  void configure(const QString &interface,bool enabled,int timeoutMs);
  bool receiving() const;
protected:
  void paintEvent(QPaintEvent*) override;
  void changeEvent(QEvent*) override;
};
}
