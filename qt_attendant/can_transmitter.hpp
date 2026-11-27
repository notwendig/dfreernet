#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>
namespace attendant {
struct TransmitFrame {
    std::uint32_t id = 0;
    bool extended = false;
    bool remote = false;
    unsigned char length = 0;
    QByteArray data;
};
// Classic SocketCAN frame syntax: 123#0102, 02000300#0000, 123#R8.
bool parse_transmit_frame(const QString &text, TransmitFrame &frame);
class CanTransmitter : public QObject {
    Q_OBJECT
public:
    explicit CanTransmitter(QObject *parent=nullptr):QObject(parent){}
    ~CanTransmitter() override;
    bool open(const QString &interface);
    bool send(const QString &text);
    void close();
    bool connected() const {return fd_ >= 0;}
    quint64 count() const {return count_;}
    QString error() const {return error_;}
    static QByteArray encode(const TransmitFrame &frame);
signals:
    void changed();
private:
    int fd_ = -1, lease_ = -1;
    quint64 count_ = 0;
    QString error_;
};
}
