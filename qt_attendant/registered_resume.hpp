#pragma once
#include "startup_config.hpp"
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QStringList>

namespace attendant {
// Reconnect to an existing slot assignment without enumeration or table writes.
// Only identity/configuration reads are sent; ownership still needs CanControl.
class RegisteredResume {
public:
    enum class State { Waiting, Fingerprint, Identities, Configuration, Flags, Profiles, Ready, Failed };
    bool configure(const QJsonObject &profile);
    void reset();
    QStringList cancel();
    void begin(qint64 now);
    QStringList receive(quint32 id,const QByteArray &data,qint64 now);
    QStringList tick(qint64 now);
    void fail(const QString &error);
    bool configured() const {return configured_;}
    bool started() const {return state_!=State::Waiting;}
    bool running() const {return state_>=State::Fingerprint && state_<=State::Profiles;}
    bool ready() const {return state_==State::Ready;}
    State state() const {return state_;}
    QString error() const {return error_;}
private:
    QHash<int,QByteArray> identities_;
    QList<QByteArray> values_;
    QByteArray flags_,profiles_;
    State state_=State::Waiting;
    StartupConfig config_;
    qint64 start_=0,deadline_=0,firstFingerprint_=-1;
    int driverFrames_=0,heartbeats_=0,identityStep_=0;
    bool pageOpen_=false,openConfirmed_=false;
    bool configured_=false;
    QString error_;
    QString identityRequest() const;
    QStringList abort(const QString &error);
};
}
