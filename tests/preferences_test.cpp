#include "app_settings.hpp"
#include "settings_dialog.hpp"
#include "can_heartbeat.hpp"
#include "i18n.hpp"
#include <QtTest>
#include <QtWidgets>
#include <QSocketNotifier>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
using namespace rnet::qt;
class PreferencesTest final : public QObject {
  Q_OBJECT
  QTemporaryDir temp_;
  AppSettings original(){auto s=AppSettings::defaults();s.workingDirectory=temp_.path()+"/documents";s.backupDirectory=temp_.path()+"/backups";s.exportDirectory=temp_.path()+"/exports";s.logDirectory=temp_.path()+"/logs";return s;}
private slots:
  void initTestCase(){QApplication::setQuitOnLastWindowClosed(false);QCoreApplication::setOrganizationName("RNetTests");QCoreApplication::setApplicationName("PreferencesTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp_.path());}
  void init(){languages().setLanguage("en_US");original().save();}
  void newDefaultsAndLiveSettings() {
    const auto defaults = original();
    QCOMPARE(defaults.accessLevel, 1);
    QVERIFY(defaults.showAllParameters);
    QVERIFY(!defaults.liveSignalEnabled);
    QCOMPARE(defaults.liveSignalIntervalSeconds, 300);
    QCOMPARE(defaults.liveSignalDurationSeconds, 60);
    QVERIFY(!defaults.toolbarIconsOnly);

    QSettings old;
    old.setValue("dongle/accessLevel", 3);
    old.remove("can/liveSignalIntervalSeconds");
    old.remove("can/liveSignalDurationSeconds");
    old.setValue("can/liveSignalIntervalMinutes", 8);
    old.setValue("can/liveSignalDurationMinutes", 2);
    auto settings = AppSettings::load();
    QCOMPARE(settings.accessLevel, 1);
    QCOMPARE(settings.liveSignalIntervalSeconds, 480);
    QCOMPARE(settings.liveSignalDurationSeconds, 120);

    settings.liveSignalEnabled = true;
    settings.showAllParameters = false;
    settings.liveSignalIntervalSeconds = 23;
    settings.liveSignalDurationSeconds = 7;
    settings.toolbarIconsOnly = true;
    settings.validated().save();
    const auto loaded = AppSettings::load();
    QVERIFY(!loaded.liveSignalEnabled);
    QSettings().setValue("can/liveSignalEnabled",true);
    QVERIFY(!AppSettings::load().liveSignalEnabled); // Legacy saved ON never auto-resumes.
    QVERIFY(!loaded.showAllParameters);
    QVERIFY(loaded.toolbarIconsOnly);
    QCOMPARE(loaded.liveSignalIntervalSeconds, 23);
    QCOMPARE(loaded.liveSignalDurationSeconds, 7);
    settings.liveSignalDurationSeconds = 23;
    QVERIFY_EXCEPTION_THROWN(settings.validated(), std::runtime_error);

    SettingsDialog dialog(defaults);
    dialog.findChild<QCheckBox*>("settingsLiveSignal")->setChecked(true);
    dialog.findChild<QSpinBox*>("settingsLiveInterval")->setValue(11);
    dialog.findChild<QSpinBox*>("settingsLiveDuration")->setValue(3);
    dialog.findChild<QCheckBox*>("settingsToolbarIconsOnly")->setChecked(true);
    dialog.findChild<QDialogButtonBox*>("settingsButtons")->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.findChild<QLabel*>("settingsError")->text().isEmpty());
    const auto applied = AppSettings::load();
    QVERIFY(!applied.liveSignalEnabled);
    QVERIFY(applied.toolbarIconsOnly);
    QCOMPARE(applied.liveSignalIntervalSeconds, 11);
    QCOMPARE(applied.liveSignalDurationSeconds, 3);
    QVERIFY(!dialog.findChild<QCheckBox*>("settingsLiveStationary"));
    QCOMPARE(dialog.findChild<QComboBox*>("settingsAccess")->count(), 2);
  }
  void roundtripValidationAndUniqueBackups(){
    auto s=original();s.canInterface="vcan42";s.accessLevel=1;s.monitor="primary";s.heartbeatTimeoutMs=750;s=s.validated();s.save();
    auto loaded=AppSettings::load();QCOMPARE(loaded.canInterface,s.canInterface);QCOMPARE(loaded.backupDirectory,s.backupDirectory);QCOMPARE(loaded.accessLevel,1);QCOMPARE(loaded.heartbeatTimeoutMs,750);
    auto a=loaded.newBackupDirectory("rnet-config"),b=loaded.newBackupDirectory("rnet-config");QVERIFY(a!=b);QVERIFY(a.startsWith(s.backupDirectory+'/'));QVERIFY(!QFileInfo::exists(a));
    s.canInterface="bad/name";QVERIFY_EXCEPTION_THROWN(s.validated(),std::runtime_error);
    s=original();s.backupDirectory="relative";QVERIFY_EXCEPTION_THROWN(s.validated(),std::runtime_error);
  }
  void verticalTabsCancelApplyAndLiveTranslation(){
    SettingsDialog cancelled(original());cancelled.show();
    auto *can=cancelled.findChild<QComboBox*>("settingsCanInterface");can->setCurrentText("vcan9");
    cancelled.findChild<QDialogButtonBox*>("settingsButtons")->button(QDialogButtonBox::Cancel)->click();
    QCOMPARE(AppSettings::load().canInterface,QString("can0"));
    SettingsDialog dialog(original());dialog.show();auto *tabs=dialog.findChild<QListWidget*>("settingsTabs");auto *pages=dialog.findChild<QStackedWidget*>("settingsPages");QCOMPARE(tabs->count(),4);
    for(int i=0;i<4;++i){tabs->setCurrentRow(i);QCOMPARE(pages->currentIndex(),i);}
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("vcan9");
    dialog.findChild<QComboBox*>("settingsLanguage")->setCurrentIndex(1);
    auto *buttons=dialog.findChild<QDialogButtonBox*>("settingsButtons");buttons->button(QDialogButtonBox::Apply)->click();
    QTRY_COMPARE(tabs->item(0)->text(),QString("Allgemein"));QCOMPARE(tabs->currentRow(),3);QCOMPARE(AppSettings::load().canInterface,QString("vcan9"));
    QCOMPARE(dialog.findChild<QComboBox*>("settingsCanInterface")->currentText(),QString("vcan9"));
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("invalid/interface");buttons->button(QDialogButtonBox::Apply)->click();QVERIFY(!dialog.findChild<QLabel*>("settingsError")->text().isEmpty());QCOMPARE(AppSettings::load().canInterface,QString("vcan9"));
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("vcan9");buttons->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.findChild<QLabel*>("settingsError")->text().isEmpty());
    for(int i=0;i<4;++i){tabs->setCurrentRow(i);QTest::qWait(20);QVERIFY(dialog.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-page-"+QString::number(i)+"-de.png"));}
    dialog.reject();languages().setLanguage("en_US");
  }
  void passiveHeartbeatOnPrivateVcan(){
    auto interface=qEnvironmentVariable("RNET_HEARTBEAT_VCAN");if(interface.isEmpty())QSKIP("Private vcan harness only");
    CanHeartbeatIndicator heart;heart.configure(interface,true,500);heart.show();QVERIFY(!heart.receiving());
    auto openSender=[&]{int fd=::socket(PF_CAN,SOCK_RAW,CAN_RAW);if(fd<0)return -1;ifreq request{};auto name=interface.toLatin1();std::memcpy(request.ifr_name,name.constData(),name.size());if(::ioctl(fd,SIOCGIFINDEX,&request)<0){::close(fd);return -1;}sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;if(::bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))){::close(fd);return -1;}return fd;};
    int fd=openSender();QVERIFY(fd>=0);
    can_frame frame{};frame.can_id=0x123;frame.can_dlc=8;QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));QTest::qWait(50);QVERIFY(!heart.receiving());
    frame.can_id=0x00e;frame.can_dlc=8;QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));QTest::qWait(50);QVERIFY(!heart.receiving());
    frame.can_id=CAN_EFF_FLAG|0x03c30f0fU;frame.can_dlc=7;
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));QTest::qWait(50);QVERIFY(!heart.receiving());
    for(int i=0;i<7;++i)frame.data[i]=0x87;
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));QTRY_VERIFY(heart.receiving());QVERIFY(heart.toolTip().contains("receive only"));
    QTest::qWait(80);QVERIFY(heart.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-heartbeat-active.png"));
    QTest::qWait(550);QVERIFY(!heart.receiving());QVERIFY(heart.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-heartbeat-stale.png"));
    auto txCount=[&](){QProcess process;process.start("ip",{"-j","-s","link","show",interface});if(!process.waitForFinished(5000)||process.exitCode())return -1;const auto rows=QJsonDocument::fromJson(process.readAllStandardOutput()).array();if(rows.isEmpty())return -1;const auto object=rows.first().toObject();const auto stats=object.value(object.contains("stats64")?"stats64":"stats").toObject();return stats.value("tx").toObject().value("packets").toInt(-1);};
    QCOMPARE(txCount(),4);::close(fd);
    // Fault injection: a USB reset may not produce a readable socket error.
    auto *notifier=heart.findChild<QSocketNotifier*>();QVERIFY(notifier);notifier->setEnabled(false);
    auto link=[&](const QStringList &args){return QProcess::execute("ip",QStringList{"link"}+args);};
    QCOMPARE(link({"set",interface,"down"}),0);QTest::qWait(1100);const bool downShown=heart.toolTip().contains("interface is down");
    QCOMPARE(link({"delete",interface}),0);QTest::qWait(100);
    QCOMPARE(link({"add",interface,"type","vcan"}),0);QCOMPARE(link({"set",interface,"up"}),0);
    QTRY_VERIFY(heart.toolTip().contains("no heartbeat"));
    fd=openSender();QVERIFY(fd>=0);QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_VERIFY(heart.receiving());QVERIFY(downShown);::close(fd);
    QTest::qWait(30);QCOMPARE(txCount(),1);
  }
};
QTEST_MAIN(PreferencesTest)
#include "preferences_test.moc"
