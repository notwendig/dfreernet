#pragma once
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QTime>
namespace rnet::qt {
constexpr quint32 rnetClockBroadcastId=0x1c2c0000U;
constexpr quint32 rnetClockSetId=0x1c2c0001U;
struct RnetClockValue {
  QDate date;
  QTime time;
  bool isValid() const { return date.isValid() && time.isValid(); }
};
RnetClockValue decodeRnetClockFrame(quint32 canId,const QByteArray &payload);
QByteArray encodeRnetClockTime(const QDateTime &hostTime);
}
