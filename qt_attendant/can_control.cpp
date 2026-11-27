#include "can_control.hpp"
#include <algorithm>

namespace attendant {
QStringList CanControl::request(bool attendant, qint64 now)
{
    QStringList frames;
    const bool wasOwned = owned();
    if (owner_ == 0) frames << "02000000#0000";
    if (owner_ == 0) {
        frames << QString("060#40") +
            QString::number(0x70 | mode_, 16).rightJustified(2, '0') + "0000";
    }
    requestedMode_ = mode_;
    target_ = attendant ? 0 : 1;
    owner_ = -1;
    acknowledged_ = false;
    suspensionAcknowledged_ = false;
    deadline_ = now + 1000;
    waitForSuspend_ = wasOwned;
    profilePending_ = !waitForSuspend_;
    profileAcknowledged_ = false;
    profileEightConfirmed_ = false;
    outgoing_.clear();
    if (!waitForSuspend_)
        frames << (attendant ? "050#00070000" : "050#00000000");
    return frames;
}
QStringList CanControl::changeMode(qint64 now)
{
    if (!owned() || pending() || !profileEightConfirmed_) return {};
    // Suspend using the old mode before selecting the new one.
    auto frames = QStringList{"02000000#0000",
        QString("060#40") + QString::number(0x70 | mode_, 16).rightJustified(2, '0') + "0000"};
    requestedMode_ = mode_ ^ 1;
    owner_ = -1;
    target_ = 0;
    acknowledged_ = false;
    suspensionAcknowledged_ = false;
    profilePending_ = false;
    waitForSuspend_ = true;
    outgoing_.clear();
    deadline_ = now + 1000;
    return frames;
}
QStringList CanControl::speed(int percent) const
{
    if (!owned() || pending() || !profileEightConfirmed_) return {};
    return {QString("0A040000#%1").arg(std::clamp(percent, 0, 100), 2, 16, QLatin1Char('0'))};
}
bool CanControl::receive(quint32 id, const QByteArray &data, qint64 now)
{
    if (data.size() != 4) return false;
    const auto *bytes = reinterpret_cast<const unsigned char *>(data.constData());
    if (id == 0x061 && bytes[0] == 0)
        passengerMode_ = bytes[1] & 15;
    if ((owned() || target_ == 0) &&
        (((id == 0x050 || id == 0x051) && bytes[0] == 0 && bytes[1] != 7) ||
         (id == 0x061 && bytes[0] == 0) ||
         (owned() && id == 0x053 && bytes[0] == 0x10 && bytes[1] != 7))) {
        reset();
        return true;
    }
    // Both return and mode changes wait for the old mode's suspend completion.
    // A new selection before this barrier produces "Mode not available".
    if (waitForSuspend_) {
        if (now >= deadline_ || id != 0x063 || bytes[1] != mode_ || bytes[2] != 0)
            return false;
        if (bytes[0] == 0x50 && bytes[3] == 2) suspensionAcknowledged_ = true;
        if (suspensionAcknowledged_ && bytes[0] == 0x80 && bytes[3] == 0x10) {
            waitForSuspend_ = false;
            deadline_ = now + 1000;
            if (target_ == 0 && profileEightConfirmed_) {
                outgoing_ << QString("060#00") +
                    QString::number(0x70 | requestedMode_,16).rightJustified(2,'0') + "0000";
            } else {
                profilePending_ = true;
                outgoing_ << (target_ == 0 ? "050#00070000" : "050#00000000");
            }
        }
        return false;
    }
    if (profilePending_) {
        if (now >= deadline_ || id != 0x053 || bytes[1] != (target_ == 0 ? 7 : 0) || bytes[2] != 0)
            return false;
        if (bytes[0] == 0x10 && bytes[3] == 2) profileAcknowledged_ = true;
        if (profileAcknowledged_ && bytes[0] == 0x80 + target_ && bytes[3] == 1) {
            profilePending_ = false;
            profileEightConfirmed_ = target_ == 0;
            deadline_ = now + 1000;
            if (target_ == 0)
                outgoing_ << QString("060#00") +
                    QString::number(0x70 | requestedMode_,16).rightJustified(2,'0') + "0000";
            // On return, the physical passenger module selects its own mode.

        }
        return false;
    }
    if (id != 0x063) return false;
    if (owned() && bytes[0] == 0x11 && bytes[2] == 0 && bytes[3] == 2) {
        reset();
        return true;
    }
    if (!pending() || now >= deadline_) return false;
    if (bytes[1] != (target_ == 0 ? requestedMode_ : passengerMode_) || bytes[2] != 0) return false;
    if (bytes[0] == 0x10 + target_ && bytes[3] == 2) acknowledged_ = true;
    if (acknowledged_ && bytes[0] == 0x80 && bytes[3] == 0x80) {
        owner_ = target_;
        if (owner_ == 0) mode_ = requestedMode_;
        target_ = -1;
        return true;
    }
    return false;
}
bool CanControl::expired(qint64 now) const
{
    return pending() && now >= deadline_;
}
QString CanControl::joystick(int x, int y) const
{
    if (!owned() || pending() || !profileEightConfirmed_) return {};
    return QString("02000000#%1%2")
        .arg(std::clamp(x, -100, 100) & 255, 2, 16, QLatin1Char('0'))
        .arg(std::clamp(y, -100, 100) & 255, 2, 16, QLatin1Char('0'));
}
void CanControl::reset()
{
    owner_ = target_ = -1;
    requestedMode_ = mode_;
    acknowledged_ = false;
    suspensionAcknowledged_ = waitForSuspend_ = false;
    profilePending_ = profileAcknowledged_ = profileEightConfirmed_ = false;
    outgoing_.clear();
}
QStringList CanControl::takeOutgoing()
{
    QStringList frames;
    frames.swap(outgoing_);
    return frames;
}
}
