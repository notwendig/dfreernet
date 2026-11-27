#pragma once
#include <QObject>
#include <QByteArray>
#include <QPointer>
class QSocketNotifier;
namespace attendant {
// Intentionally receive-only. No write/send method or CAN control-frame encoder.
class CanObserver : public QObject {
    Q_OBJECT
public:
    explicit CanObserver(QObject *parent=nullptr):QObject(parent){}
    ~CanObserver() override;
    bool open(const QString &interface);
    void close();
    bool checkInterface();
    bool connected() const {return fd_>=0;}
    quint64 count() const{return count_;}
    QString error() const{return error_;}
signals:
    void changed();
    void firstDataFrame();
    void powerOffRequested();
    void protocolFrame(quint32 id, const QByteArray &data);
    void dataFrame(quint32 id, const QByteArray &data);
private:
    int fd_=-1, interfaceIndex_=0;
    bool receivedData_ = false;
    quint64 count_=0;
    QString error_;
    QPointer<QSocketNotifier> notifier_;
    void read();
};
}
