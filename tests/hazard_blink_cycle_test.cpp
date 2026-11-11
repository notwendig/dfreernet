#include "hazard_blink_cycle.hpp"
#include <QtTest>
#include <QSocketNotifier>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
namespace rnet::qt {
class HazardBlinkCycleTest final:public QObject {
  Q_OBJECT
private slots:
  void configurableTimingIsValidatedAndFinishesPendingPair(){
    std::vector<bool> sent;HazardBlinkCycle cycle(nullptr,[&](bool first){sent.push_back(first);return true;});cycle.canPulse=[]{return true;};QCOMPARE(cycle.intervalMs(),300000);QCOMPARE(cycle.durationMs(),60000);
    QVERIFY(!cycle.configure(0,1));QVERIFY(!cycle.configure(100,100));QVERIFY(sent.empty());QVERIFY(cycle.configure(480000,120000));cycle.start("rnetTest");QCOMPARE(sent.size(),std::size_t(1));QVERIFY(cycle.pending());
    QVERIFY(cycle.configure(300000,60000));QCOMPARE(sent.size(),std::size_t(2));QVERIFY(cycle.active());QVERIFY(!cycle.pending());QVERIFY(sent[0]&&!sent[1]);cycle.stop();QCOMPARE(sent.size(),std::size_t(2));
  }
  void missingHeartbeatStopsWithoutAutoResume(){
    bool ready=false;std::vector<bool> sent;HazardBlinkCycle cycle(nullptr,[&](bool first){sent.push_back(first);return true;});cycle.canPulse=[&]{return ready;};cycle.pulseMs_=1000;
    cycle.start("rnetTest");QVERIFY(!cycle.active());QVERIFY(sent.empty());
    ready=true;QTest::qWait(150);QVERIFY(sent.empty());
    cycle.start("rnetTest");QCOMPARE(sent.size(),std::size_t(1));QVERIFY(cycle.pending());
    ready=false;QTRY_VERIFY_WITH_TIMEOUT(!cycle.active(),1000);QCOMPARE(sent.size(),std::size_t(2));
    ready=true;QTest::qWait(150);QCOMPARE(sent.size(),std::size_t(2));
  }
  void startsImmediatelyAndPairsToggles(){
    std::vector<bool> sent;HazardBlinkCycle cycle(nullptr,[&](bool first){sent.push_back(first);return true;});cycle.canPulse=[]{return true;};cycle.pulseMs_=1;cycle.start("rnetTest");QCOMPARE(sent.size(),std::size_t(1));QCOMPARE(cycle.interval_.interval(),300000);cycle.pulse();QTRY_COMPARE(sent.size(),std::size_t(2));QVERIFY(sent[0]&&!sent[1]);cycle.stop();QTest::qWait(10);QCOMPARE(sent.size(),std::size_t(2));
  }
  void pauseAndDestructionFinishPendingPair(){
    std::vector<bool> sent;{HazardBlinkCycle cycle(nullptr,[&](bool first){sent.push_back(first);return true;});cycle.canPulse=[]{return true;};cycle.start("rnetTest");cycle.pulse();cycle.quiesce();QCOMPARE(sent.size(),std::size_t(2));QVERIFY(cycle.active());cycle.pulse();}QCOMPARE(sent.size(),std::size_t(4));QVERIFY(sent[2]&&!sent[3]);
  }
  void sendFailuresNeverRetry(){
    int calls=0;HazardBlinkCycle cycle(nullptr,[&](bool){++calls;return false;});cycle.canPulse=[]{return true;};cycle.start("rnetTest");QCOMPARE(calls,1);QVERIFY(!cycle.active());QVERIFY(!cycle.error().isEmpty());cycle.pulse();QCOMPARE(calls,1);
    std::vector<bool> attempts;HazardBlinkCycle failedEnd(nullptr,[&](bool first){attempts.push_back(first);return first;});failedEnd.canPulse=[]{return true;};failedEnd.start("rnetTest");failedEnd.pulse();failedEnd.stop();QCOMPARE(attempts.size(),std::size_t(2));QVERIFY(!failedEnd.active());QVERIFY(!failedEnd.error().isEmpty());failedEnd.stop();QCOMPARE(attempts.size(),std::size_t(2));
  }
  void realPairOnPrivateVcan(){
    auto interface=qEnvironmentVariable("RNET_BLINK_VCAN");if(interface.isEmpty())QSKIP("Isolated vcan harness only");
    int receiver=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK,CAN_RAW);QVERIFY(receiver>=0);sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=::if_nametoindex(interface.toLatin1().constData());QVERIFY(address.can_ifindex>0);QVERIFY(::bind(receiver,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
    std::vector<can_frame> frames;QSocketNotifier notifier(receiver,QSocketNotifier::Read);connect(&notifier,&QSocketNotifier::activated,this,[&]{can_frame f;while(::read(receiver,&f,sizeof(f))==sizeof(f))frames.push_back(f);});
    HazardBlinkCycle cycle;cycle.canPulse=[]{return true;};cycle.pulseMs_=10;cycle.start(interface);cycle.pulse();QTRY_COMPARE(frames.size(),std::size_t(2));cycle.stop();for(const auto &f:frames){QCOMPARE(f.can_id,canid_t(CAN_EFF_FLAG|0x0c000403));QCOMPARE(f.can_dlc,0);}::close(receiver);
  }
};
}
QTEST_GUILESS_MAIN(rnet::qt::HazardBlinkCycleTest)
#include "hazard_blink_cycle_test.moc"
