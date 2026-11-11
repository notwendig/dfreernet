#include "sleep_inhibitor.hpp"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QDBusContext>
#include <QDBusMessage>
#include <fcntl.h>
#include <unistd.h>
using namespace rnet::qt;
class FakeLogind final:public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface","org.freedesktop.login1.Manager")
public:
  std::vector<int> readers;QString lastWhat;int calls=0;
  ~FakeLogind(){for(int fd:readers)::close(fd);}
  bool released()const{for(int fd:readers){char b;if(::read(fd,&b,1)!=0)return false;}return true;}
public slots:
  QDBusUnixFileDescriptor Inhibit(const QString &what,const QString &who,const QString &,const QString &mode){
    ++calls;lastWhat=what;if(who!="dfreernet"||mode!="block")return {};
    int pair[2];if(::pipe2(pair,O_NONBLOCK|O_CLOEXEC))return {};readers.push_back(pair[0]);QDBusUnixFileDescriptor result(pair[1]);::close(pair[1]);return result;
  }
};
class FakeIdle final:public QObject,protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface","org.freedesktop.ScreenSaver")
public:
  int calls=0,releases=0;quint32 cookie=0;bool delayed=false;QDBusMessage pending;
  void answer(){QDBusConnection::sessionBus().send(pending.createReply(QList<QVariant>{QVariant::fromValue(cookie)}));}
public slots:
  quint32 Inhibit(const QString &,const QString &){++calls;++cookie;if(delayed){setDelayedReply(true);pending=message();}return cookie;}
  void UnInhibit(quint32 value){if(value==cookie)++releases;}
};
class SleepTest final:public QObject {
  Q_OBJECT
  QDBusConnection bus=QDBusConnection::sessionBus();QDBusConnection client=QDBusConnection::connectToBus(QDBusConnection::SessionBus,"dfreernet-sleep-test-client");FakeLogind logind;FakeIdle idle;
private slots:
  void initTestCase(){QVERIFY(bus.isConnected());QVERIFY(client.isConnected());QVERIFY(bus.registerService("org.freedesktop.login1"));QVERIFY(bus.registerService("org.freedesktop.ScreenSaver"));QVERIFY(bus.registerObject("/org/freedesktop/login1",&logind,QDBusConnection::ExportAllSlots));QVERIFY(bus.registerObject("/org/freedesktop/ScreenSaver",&idle,QDBusConnection::ExportAllSlots));}
  void holdsAndReleasesLocks(){
    SleepInhibitor lock(nullptr,client,client);lock.configure(true);QTRY_VERIFY(lock.sleepBlocked());QTRY_VERIFY(lock.idleBlocked());QCOMPARE(logind.lastWhat,QString("sleep:idle"));QCOMPARE(logind.calls,1);QCOMPARE(idle.calls,1);
    lock.configure(true);QTest::qWait(30);QCOMPARE(logind.calls,1);QCOMPARE(idle.calls,1);QVERIFY(!logind.released());
    lock.configure(false);QVERIFY(!lock.sleepBlocked());QVERIFY(!lock.idleBlocked());QTRY_COMPARE(idle.releases,1);QTRY_VERIFY(logind.released());
  }
  void lateRepliesAreReleased(){
    idle.delayed=true;SleepInhibitor lock(nullptr,client,client);lock.configure(true);QTRY_COMPARE(idle.calls,2);lock.configure(false);idle.answer();QTRY_COMPARE(idle.releases,2);QTRY_VERIFY(logind.released());QVERIFY(!lock.idleBlocked());idle.delayed=false;
  }
  void destroyedOwnerDoesNotLeakCookie(){
    idle.delayed=true;auto *lock=new SleepInhibitor(nullptr,client,client);lock->configure(true);QTRY_COMPARE(idle.calls,3);delete lock;idle.answer();QTRY_COMPARE(idle.releases,3);QTRY_VERIFY(logind.released());idle.delayed=false;
  }
};
QTEST_GUILESS_MAIN(SleepTest)
#include "sleep_inhibitor_test.moc"
