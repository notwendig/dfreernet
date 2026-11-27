#pragma once
#include <QHash>
#include <QJsonArray>
#include <QVector>
#include <QStringList>

namespace attendant {
// Experimental slot ownership inferred from the identity-bearing F060 records.
// Only slot 0 is emitted; captured peers are required at every stage boundary.
class StartupTables {
public:
    bool configure(const QJsonArray &records);
    void reset();
    void begin(qint64 now);
    void receive(quint32 id, const QByteArray &data);
    QStringList tick(qint64 now);
    bool configured() const { return !stages_.isEmpty(); }
    bool complete() const { return complete_; }
    QString error() const { return error_; }
private:
    struct Record { quint32 id; QByteArray data; qint64 offset; };
    struct Stage { QVector<Record> own, peers; qint64 duration = 0; };
    QVector<Stage> stages_;
    QVector<QVector<bool>> seen_;
    int stage_ = -1, sent_ = 0;
    qint64 started_ = 0, overallDeadline_ = 0;
    bool complete_ = false;
    QString error_;
};
}
