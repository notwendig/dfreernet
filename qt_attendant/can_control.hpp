#pragma once
#include <QStringList>
#include <QByteArray>

namespace attendant {
// Control exchange observed in ctrlsys.txt, with attendant input at slot 0.
class CanControl {
public:
    QStringList request(bool attendant, qint64 now);
    QStringList changeMode(qint64 now);
    QStringList speed(int percent) const;
    bool receive(quint32 id, const QByteArray &data, qint64 now);
    bool expired(qint64 now) const;
    QString joystick(int x, int y) const;
    void reset();
    QStringList takeOutgoing();
    bool owned() const { return owner_ == 0; }
    bool returning() const { return target_ == 1; }
    bool pending() const { return target_ >= 0; }
    int mode() const { return mode_; }
private:
    int owner_ = -1;
    int target_ = -1;
    int mode_ = 0, requestedMode_ = 0;
    int passengerMode_ = 0;
    bool acknowledged_ = false;
    bool suspensionAcknowledged_ = false, waitForSuspend_ = false;
    bool profilePending_ = false;
    bool profileAcknowledged_ = false;
    bool profileEightConfirmed_ = false;
    QStringList outgoing_;
    qint64 deadline_ = 0;
};
}
