#pragma once
#include <QByteArray>
#include <QString>

namespace attendant {
// Slot-0 configuration reads observed in insys.txt, lines 311–325.
// Enumeration has to establish the input slot before beginning this exchange.
class StartupConfig {
public:
    enum class State { Waiting, Loading, Ready, Failed };
    QString begin(qint64 now);
    QString receive(quint32 id, const QByteArray &data, qint64 now);
    void tick(qint64 now);
    void reset();
    State state() const { return state_; }
    QString error() const { return error_; }
    QByteArray value(int index) const;
private:
    int step_ = 0;
    State state_ = State::Waiting;
    qint64 deadline_ = 0;
    QString error_;
    QByteArray values_[7];
};
}
