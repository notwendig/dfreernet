#pragma once
#include "startup_tables.hpp"
#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

namespace attendant {
// Capture-specific startup exchange. Acceptance is established by the subsequent
// slot-0 POP exchange, not by successful transmission of these announcements.
class ModuleEnrollment {
public:
    bool configure(const QJsonObject &profile);
    void boot(qint64 now);
    void allowAutomaticEnrollment(bool enabled) { automatic_ = enabled; }
    bool liveProfileReady() const { return configured() && tables_.configured() && expectedIdentities_.size() == 5; }
    void requestInitialEnrollment() { initialRequested_ = true; }
    QStringList tick(qint64 now);
    QStringList receive(quint32 id, const QByteArray &data, qint64 now);
    void reset();
    void fail(const QString &reason);
    bool running() const { return running_; }
    bool configured() const { return serial_.size() == 8; }
    bool registeredRestart() const { return normalConfirmed_; }
    bool complete() const { return normalConfirmed_ || tables_.complete(); }
    bool configurationRequested() const { return configurationRequested_; }
    QString error() const { return error_; }
private:
    StartupTables tables_;
    QHash<unsigned,QByteArray> expectedIdentities_, receivedIdentities_;
    bool tablesStarted_ = false;
    bool automatic_ = true;
    unsigned serialParts_ = 0;
    bool initialRequested_ = false, initialBoot_ = false, normalConfirmed_ = false, discoveryEnded_ = false;
    QHash<unsigned,QByteArray> normalPeers_;
    QByteArray serial_, keys_;
    QSet<quint16> expectedWords_, receivedWords_;
    bool running_ = false, advertising_ = false, enumerated_ = false, metadataSent_ = false;
    bool configurationRequested_ = false;
    qint64 nextAdvertisement_ = 0, deadline_ = 0, normalReadyAt_ = 0;
    int responses_ = 0, advertisements_ = 0;
    QString error_;
};
}
