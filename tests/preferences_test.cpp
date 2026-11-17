#include "app_settings.hpp"
#include "settings_dialog.hpp"
#include "can_heartbeat.hpp"
#include "rnet_clock.hpp"
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
  void rnetClockFrame(){
    auto clock=decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0000U,QByteArray::fromHex("281604ef0b1a"));
    QVERIFY(clock.isValid());QCOMPARE(clock.date,QDate(2026,11,15));QCOMPARE(clock.time,QTime(4,22,40));
    clock=decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0300U,QByteArray::fromHex("1336144c0710"));
    QVERIFY(clock.isValid());QCOMPARE(clock.date,QDate(2016,7,12));QCOMPARE(clock.time,QTime(20,54,19));
    clock=decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0000U,QByteArray::fromHex("002e154f0b1a"));
    QVERIFY(clock.isValid());QCOMPARE(clock.date,QDate(2026,11,15));QCOMPARE(clock.time,QTime(21,46,0));
    QCOMPARE(encodeRnetClockTime(QDateTime(QDate(2026,11,15),QTime(3,50,12))),QByteArray::fromHex("0c3203ef0b1a"));
    QVERIFY(!decodeRnetClockFrame(0x1c2c0000U,QByteArray::fromHex("281604ef0b1a")).isValid());
    QVERIFY(!decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0000U,QByteArray::fromHex("281604ef0b")).isValid());
    QVERIFY(!decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0000U,QByteArray::fromHex("281604e00b1a")).isValid());
  }
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
    QVERIFY(!defaults.configureCanOnStartup);
    QCOMPARE(defaults.canBitrate,125000);

    QSettings old;
    old.setValue("ui/monitor","primary");
    old.setValue("ui/maximized",false);
    old.setValue("dongle/accessLevel", 3);
    old.remove("can/liveSignalIntervalSeconds");
    old.remove("can/liveSignalDurationSeconds");
    old.setValue("can/liveSignalIntervalMinutes", 8);
    old.setValue("can/liveSignalDurationMinutes", 2);
    auto settings = AppSettings::load();
    QVERIFY(!old.contains("ui/monitor"));
    QVERIFY(!old.contains("ui/maximized"));
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
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, settings.validated());

    SettingsDialog dialog(defaults);
    QVERIFY(!dialog.findChild<QComboBox*>("settingsMonitor"));
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
    auto s=original();s.canInterface="vcan42";s.canBitrate=250000;s.configureCanOnStartup=true;s.accessLevel=1;s.heartbeatTimeoutMs=750;s=s.validated();s.save();
    QVERIFY(!QSettings().contains("ui/monitor"));
    QVERIFY(!QSettings().contains("ui/maximized"));
    auto loaded=AppSettings::load();QCOMPARE(loaded.canInterface,s.canInterface);QCOMPARE(loaded.canBitrate,250000);QVERIFY(loaded.configureCanOnStartup);QCOMPARE(loaded.backupDirectory,s.backupDirectory);QCOMPARE(loaded.accessLevel,1);QCOMPARE(loaded.heartbeatTimeoutMs,750);
    auto a=loaded.newBackupDirectory("rnet-config"),b=loaded.newBackupDirectory("rnet-config");QVERIFY(a!=b);QVERIFY(a.startsWith(s.backupDirectory+'/'));QVERIFY(!QFileInfo::exists(a));
    s.canInterface="bad/name";QVERIFY_THROWS_EXCEPTION(std::runtime_error,s.validated());
    s=original();s.canBitrate=0;QVERIFY_THROWS_EXCEPTION(std::runtime_error,s.validated());
    s=original();s.backupDirectory="relative";QVERIFY_THROWS_EXCEPTION(std::runtime_error,s.validated());
  }
  void verticalTabsCancelApplyAndLiveTranslation(){
    SettingsDialog cancelled(original());cancelled.show();
    auto *can=cancelled.findChild<QComboBox*>("settingsCanInterface");can->setCurrentText("vcan9");
    cancelled.findChild<QDialogButtonBox*>("settingsButtons")->button(QDialogButtonBox::Cancel)->click();
    QCOMPARE(AppSettings::load().canInterface,QString("can0"));
    SettingsDialog dialog(original());dialog.show();auto *tabs=dialog.findChild<QListWidget*>("settingsTabs");auto *pages=dialog.findChild<QStackedWidget*>("settingsPages");QCOMPARE(tabs->count(),4);
    for(int i=0;i<4;++i){tabs->setCurrentRow(i);QCOMPARE(pages->currentIndex(),i);}
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("vcan9");
    dialog.findChild<QCheckBox*>("settingsConfigureCanOnStartup")->setChecked(true);
    dialog.findChild<QSpinBox*>("settingsCanBitrate")->setValue(500000);
    dialog.findChild<QComboBox*>("settingsLanguage")->setCurrentIndex(1);
    auto *buttons=dialog.findChild<QDialogButtonBox*>("settingsButtons");buttons->button(QDialogButtonBox::Apply)->click();
    QTRY_COMPARE(tabs->item(0)->text(),QString("Allgemein"));QCOMPARE(tabs->currentRow(),3);QCOMPARE(AppSettings::load().canInterface,QString("vcan9"));QCOMPARE(AppSettings::load().canBitrate,500000);QVERIFY(AppSettings::load().configureCanOnStartup);
    QCOMPARE(dialog.findChild<QComboBox*>("settingsCanInterface")->currentText(),QString("vcan9"));
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("invalid/interface");buttons->button(QDialogButtonBox::Apply)->click();QVERIFY(!dialog.findChild<QLabel*>("settingsError")->text().isEmpty());QCOMPARE(AppSettings::load().canInterface,QString("vcan9"));
    dialog.findChild<QComboBox*>("settingsCanInterface")->setCurrentText("vcan9");buttons->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.findChild<QLabel*>("settingsError")->text().isEmpty());
    for(int i=0;i<4;++i){tabs->setCurrentRow(i);QTest::qWait(20);QVERIFY(dialog.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-page-"+QString::number(i)+"-de.png"));}
    dialog.reject();languages().setLanguage("en_US");
  }
  void heartbeatConsumesSharedFrames(){
    CanHeartbeatIndicator heart;heart.configure("rnetOffline",true,500);heart.show();
    heart.setSocketState(true);
    can_frame frame{};frame.can_id=CAN_EFF_FLAG|0x03c30f0fU;frame.can_dlc=7;
    heart.observe(frame);QVERIFY(!heart.receiving());
    for(int i=0;i<7;++i)frame.data[i]=0x87;
    heart.observe(frame);QVERIFY(heart.receiving());
    QVERIFY(heart.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-heartbeat-active.png"));
    heart.setSocketState(false,true);QVERIFY(!heart.receiving());QVERIFY(heart.toolTip().contains("interface is down"));
    heart.setSocketState(true);QVERIFY(!heart.receiving());
    heart.observe(frame);QVERIFY(heart.receiving());
    QTest::qWait(550);QVERIFY(!heart.receiving());
    QVERIFY(heart.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/settings-heartbeat-stale.png"));
  }
};
QTEST_MAIN(PreferencesTest)
#include "preferences_test.moc"
