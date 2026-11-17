#include "can_send_queue.hpp"
#include <QtTest>
#include <atomic>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
namespace rnet::qt {
class CanSendQueueTest final : public QObject {
  Q_OBJECT
private slots:
  void serializesConcurrentSendersAndReads(){
    int sockets[2]={-1,-1};
    QVERIFY(::socketpair(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK,0,sockets)==0);
    CanSendQueue queue;
    queue.attach(sockets[0]);
    std::atomic<int> failures{0};
    auto producer=[&](int source){
      for(int number=0;number<20;++number){
        can_frame frame{};frame.can_id=CAN_EFF_FLAG|0x123;frame.can_dlc=2;
        frame.data[0]=quint8(source);frame.data[1]=quint8(number);
        if(queue.send(frame).bytes!=ssize_t(sizeof(frame)))++failures;
      }
    };
    std::thread first(producer,0),second(producer,1);
    int next[2]={0,0},orderFailures=0;
    QElapsedTimer deadline;deadline.start();
    while(next[0]+next[1]<40&&deadline.elapsed()<3000){
      can_frame frame{};
      if(::read(sockets[1],&frame,sizeof(frame))!=ssize_t(sizeof(frame))){QTest::qWait(1);continue;}
      if(frame.data[0]>=2){++orderFailures;continue;}
      if(int(frame.data[1])!=next[frame.data[0]])++orderFailures;
      ++next[frame.data[0]];
    }
    first.join();second.join();
    QCOMPARE(failures.load(),0);
    QCOMPARE(orderFailures,0);
    QCOMPARE(next[0],20);QCOMPARE(next[1],20);
    can_frame inbound{};inbound.can_id=CAN_EFF_FLAG|0x321;
    QCOMPARE(::write(sockets[1],&inbound,sizeof(inbound)),ssize_t(sizeof(inbound)));
    can_frame received{};
    QCOMPARE(queue.read(received),ssize_t(sizeof(received)));
    QCOMPARE(received.can_id,inbound.can_id);
    queue.detach();
    QCOMPARE(queue.send(inbound).error,ENODEV);
    ::close(sockets[0]);::close(sockets[1]);
  }
};
}
QTEST_APPLESS_MAIN(rnet::qt::CanSendQueueTest)
#include "can_send_queue_test.moc"
