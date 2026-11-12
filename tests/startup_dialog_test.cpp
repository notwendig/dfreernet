#include "startup_dialog.hpp"
#include "i18n.hpp"
#include <QtTest>
#include <QtWidgets>
using namespace rnet::qt;
class StartupDialogTest final : public QObject {
  Q_OBJECT
  QTemporaryDir settings_;
private slots:
  void initTestCase(){
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName("RNetTests");QCoreApplication::setApplicationName("StartupTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
  }
  void exitRejectsActualModal(){
    languages().setLanguage("en_US");StartupDialog dialog;
    QCOMPARE(dialog.windowModality(),Qt::ApplicationModal);
    QVERIFY(dialog.findChild<QPushButton*>("startupExit")->isDefault());
    QTimer::singleShot(0,&dialog,[&]{
      QCOMPARE(QApplication::activeModalWidget(),&dialog);
      QTest::mouseClick(dialog.findChild<QPushButton*>("startupExit"),Qt::LeftButton);
    });
    QCOMPARE(dialog.exec(),int(QDialog::Rejected));
  }
  void continueRequiresExplicitChoiceEveryTime(){
    StartupDialog first;
    QTimer::singleShot(0,&first,[&]{QTest::mouseClick(first.findChild<QPushButton*>("startupContinue"),Qt::LeftButton);});
    QCOMPARE(first.exec(),int(QDialog::Accepted));
    StartupDialog again;QCOMPARE(again.result(),int(QDialog::Rejected));
    QTimer::singleShot(0,&again,[&]{QTest::keyClick(&again,Qt::Key_Escape);});
    QCOMPARE(again.exec(),int(QDialog::Rejected));
  }
  void defaultEnterExits(){
    StartupDialog dialog;
    QTimer::singleShot(20,&dialog,[&]{QTest::keyClick(&dialog,Qt::Key_Return);});
    QCOMPARE(dialog.exec(),int(QDialog::Rejected));
  }
  void realEntryPointRejectsBeforeOpeningConfigurationOrCAN(){
    QTemporaryDir temporary;auto output=temporary.path()+"/must-not-be-created";
    QProcess process;
    process.start(QCoreApplication::applicationDirPath()+"/dfreernet",{
      "--document",temporary.path()+"/missing.R-net",
      "--output",output,
      "--read-controller","not-a-real-CAN-interface",
      "--stationary-confirmed","--backup-directory",temporary.path()+"/no-CAN-backup",
      "--language","en_US","--quit-after","1500",
      "--screenshot",temporary.path()+"/notice.png"});
    QVERIFY(process.waitForFinished(6000));
    QCOMPARE(process.exitStatus(),QProcess::NormalExit);QCOMPARE(process.exitCode(),0);
    QVERIFY(QFileInfo::exists(temporary.path()+"/notice.png"));
    QVERIFY(!QFileInfo::exists(output));
    QVERIFY(!QFileInfo::exists(temporary.path()+"/no-CAN-backup"));
    QVERIFY(!QString::fromUtf8(process.readAllStandardError()).contains("could not start"));
  }
  void englishAndGermanLayout(){
    for(const auto &code:{QString("en_US"),QString("de_DE")}){
      languages().setLanguage(code);StartupDialog dialog;dialog.placeOnSecondary();dialog.show();QTest::qWait(40);
      QVERIFY(dialog.windowTitle().contains(QStringLiteral(DFREERNET_VERSION)));
      QVERIFY(dialog.findChild<QLabel*>("startupTitle")->text().contains(QStringLiteral(DFREERNET_VERSION)));
      auto text=dialog.findChild<QTextBrowser*>("startupNoticeText")->toPlainText();
      const auto *illustration=dialog.findChild<QLabel*>("startupIllustration");
      QVERIFY(illustration);QVERIFY(!illustration->pixmap().isNull());
      if(code=="en_US"){
        QVERIFY(text.contains("unintended movement"));QVERIFY(text.contains("motors and control electronics"));QVERIFY(text.contains("Mandatory statutory liability"));
        QCOMPARE(dialog.findChild<QPushButton*>("startupContinue")->text(),QString("I understand — Continue"));
      }else{
        QVERIFY(text.contains("unbeabsichtigte Bewegungen"));QVERIFY(text.contains("Motoren und Steuerungselektronik"));QVERIFY(text.contains("Haftungsregelungen bleiben unberührt"));
        QCOMPARE(dialog.findChild<QPushButton*>("startupExit")->text(),QString("Beenden"));
      }
      QVERIFY(dialog.grab().save(QString::fromUtf8(RNET_SOURCE_DIR)+"/analysis/startup-warning-"+code+".png"));
      dialog.reject();
    }
    languages().setLanguage("en_US");
  }
};
QTEST_MAIN(StartupDialogTest)
#include "startup_dialog_test.moc"
