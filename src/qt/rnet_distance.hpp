#pragma once
#include <QByteArray>
#include <QtGlobal>
namespace rnet::qt {
constexpr quint32 rnetDistanceBroadcastId=0x1c300204U;
constexpr quint32 rnetClearTripId=0x18300f05U;
constexpr quint32 rnetClearTotalId=0x18300f06U;
struct RnetDistanceValue {
  quint32 totalMetres=0;
  quint32 tripMetres=0;
  bool valid=false;
};
RnetDistanceValue decodeRnetDistanceFrame(quint32 canId,const QByteArray &payload);
}
