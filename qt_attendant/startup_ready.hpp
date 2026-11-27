#pragma once
#include <QByteArray>
#include <QStringList>

namespace attendant {
// Capture-derived normal-start completion, separate from initial registration.
class StartupReady {
public:
    enum class State { Waiting, ReadingFlags, ReadingMode, WaitingPeers, Ready, Failed };
    void reset();
    QStringList begin(qint64 now);
    QStringList receive(quint32 id, const QByteArray &data, qint64 now);
    void tick(qint64 now);
    State state() const { return state_; }
    bool ready() const { return state_ == State::Ready; }
    bool running() const { return state_ == State::ReadingFlags || state_ == State::ReadingMode || state_ == State::WaitingPeers; }
    QString error() const { return error_; }
private:
    QStringList advance();
    State state_ = State::Waiting;
    unsigned initialized_ = 0, readyPeers_ = 0;
    bool announced_ = false;
    qint64 deadline_ = 0;
    QString error_;
};
}
