#pragma once
#include <QWidget>
#include <QElapsedTimer>
class QTimer;
struct can_frame;
namespace rnet::qt {
// Passive observer: no send/write, repository ownership or controller commands.
class CanHeartbeatIndicator final : public QWidget {
  bool interfaceDown_=false,socketAvailable_=false;
  QTimer *timer_;
  QString interface_,status_;
  bool enabled_=true;
  int timeoutMs_=1500;
  QElapsedTimer lastFrame_,animation_;
  void refresh();
public:
  explicit CanHeartbeatIndicator(QWidget *parent=nullptr);
  void configure(const QString &interface,bool enabled,int timeoutMs);
  void setSocketState(bool available,bool interfaceDown=false);
  void observe(const can_frame &frame);
  bool receiving() const;
protected:
  void paintEvent(QPaintEvent*) override;
  void changeEvent(QEvent*) override;
};
}
