#include "rnet_distance.hpp"
#include <QtTest>
#include <linux/can.h>
namespace rnet::qt {
class RnetDistanceTest final : public QObject {
  Q_OBJECT
private slots:
  void decodesTotalAndTripMetres(){
    const auto value=decodeRnetDistanceFrame(CAN_EFF_FLAG|rnetDistanceBroadcastId,
                                            QByteArray::fromHex("3D5B250078030000"));
    QVERIFY(value.valid);
    QCOMPARE(value.totalMetres,quint32(0x255b3d));
    QCOMPARE(value.tripMetres,quint32(888));
  }
  void rejectsOtherFrames(){
    const auto payload=QByteArray::fromHex("F2F60500F2F60500");
    QVERIFY(!decodeRnetDistanceFrame(rnetDistanceBroadcastId,payload).valid);
    QVERIFY(!decodeRnetDistanceFrame(CAN_EFF_FLAG|CAN_RTR_FLAG|rnetDistanceBroadcastId,payload).valid);
    QVERIFY(!decodeRnetDistanceFrame(CAN_EFF_FLAG|0x1c300205U,payload).valid);
    QVERIFY(!decodeRnetDistanceFrame(CAN_EFF_FLAG|rnetDistanceBroadcastId,payload.left(7)).valid);
    QVERIFY(!decodeRnetDistanceFrame(CAN_EFF_FLAG|rnetDistanceBroadcastId,
                                     QByteArray::fromHex("FFFFFFFF00000000")).valid);
  }
};
}
QTEST_APPLESS_MAIN(rnet::qt::RnetDistanceTest)
#include "rnet_distance_test.moc"
