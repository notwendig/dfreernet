#include "tray_controller.hpp"
#include "attendant_controller.hpp"
#include <QAction>
#include <QApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest>

using namespace attendant;
class ReturnPendingWindow : public QQuickWindow {
public:
    bool returnConfirmed=false;
protected:
    bool event(QEvent *event) override {
        if(event->type()==QEvent::Close && !returnConfirmed){event->ignore();return true;}
        return QQuickWindow::event(event);
    }
};
class TrayTest : public QObject {
    Q_OBJECT
    QAction *action(TrayController &tray,const char *name) {
        return tray.menu()->findChild<QAction *>(QString::fromLatin1(name));
    }
private slots:
    void initTestCase(){QApplication::setQuitOnLastWindowClosed(false);}
    void unavailableTrayDoesNotHideWindow(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;QQuickWindow window;window.show();
        QTest::ignoreMessage(QtWarningMsg,"System tray unavailable; keeping the window visible.");
        TrayController tray(&window,&control,settings,{true,true,false},[]{return false;});
        QVERIFY(!tray.available());QVERIFY(window.isVisible());
        tray.hideWindow();QVERIFY(window.isVisible());
        QTest::ignoreMessage(QtWarningMsg,"System tray unavailable; keeping the window visible.");
        QVERIFY(!window.close());QVERIFY(window.isVisible());
    }
    void hideRestoreMinimizeAndHostLoss(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;control.registerOnDataFrame();control.toggleDrivingControl();control.setAxes(100,0);
        QQuickWindow window;window.show();bool host=true;
        QSignalSpy closed(qApp,&QGuiApplication::lastWindowClosed);
        TrayController tray(&window,&control,settings,{true,true,false},[&]{return host;});
        QVERIFY(tray.available());QVERIFY(!window.isVisible());QCOMPARE(control.outputX(),0);
        QCOMPARE(closed.count(),0);
        QVERIFY(control.attendantControl()); // hiding is independent of explicit suspension
        action(tray,"trayShow")->trigger();QVERIFY(window.isVisible());
        window.showMinimized();QTRY_VERIFY(!window.isVisible());
        host=false;
        QTest::ignoreMessage(QtWarningMsg,"System tray disappeared; restoring the window.");
        tray.checkAvailability();QVERIFY(window.isVisible());QVERIFY(!tray.available());
    }
    void suspendActionReturnsControlAndResumeDoesNotTakeOver(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;control.registerOnDataFrame();control.toggleDrivingControl();
        QQuickWindow window;window.show();
        TrayController tray(&window,&control,settings,{},[]{return true;});
        const auto serial=control.benchSerial();auto *pause=action(tray,"traySuspend");QVERIFY(pause);
        pause->trigger();QVERIFY(control.controlSuspended());QCOMPARE(pause->text(),QString("Resume control"));
        QVERIFY(!control.attendantControl());QCOMPARE(control.benchSerial(),serial);
        QVERIFY(window.isVisible()); // hide is deferred outside the controller signal
        QTRY_VERIFY(!window.isVisible());
        pause->trigger();QVERIFY(!control.controlSuspended());QVERIFY(!control.attendantControl());
        QVERIFY(window.isVisible());
        control.setControlSuspended(true);QCOMPARE(pause->text(),QString("Resume control"));
    }
    void closeMainWindowOnlyHidesAndMenuQuitActuallyCloses(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;control.registerOnDataFrame();control.toggleDrivingControl();
        const auto serial=control.benchSerial();
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("controller",&control);
        engine.load(QUrl::fromLocalFile(TRAY_TEST_QML));QVERIFY(!engine.rootObjects().isEmpty());
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());QVERIFY(window);
        TrayController tray(window,&control,settings,{},[]{return true;});
        QSignalSpy quit(&tray,&TrayController::quitReady);
        QVERIFY(!window->close());QVERIFY(!window->isVisible());QCOMPARE(quit.count(),0);
        QTest::qWait(50);QCOMPARE(control.benchSerial(),serial);QVERIFY(control.attendantControl());
        tray.showWindow();QVERIFY(window->isVisible());
        action(tray,"trayQuit")->trigger();QCOMPARE(quit.count(),1);
        QVERIFY(!window->isVisible());QVERIFY(control.benchSerial().isEmpty());
    }
    void startHiddenWaitsForLateDesktopTray(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;QQuickWindow window;window.show();bool host=false;
        QTest::ignoreMessage(QtWarningMsg,"System tray unavailable; keeping the window visible.");
        TrayController tray(&window,&control,settings,{true,true,false},[&]{return host;});
        QVERIFY(window.isVisible());host=true;tray.checkAvailability();QVERIFY(!window.isVisible());
    }
    void explicitlyShowingWindowCancelsDeferredSuspendHide(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;control.registerOnDataFrame();control.toggleDrivingControl();
        QQuickWindow window;window.show();
        TrayController tray(&window,&control,settings,{},[]{return true;});
        action(tray,"traySuspend")->trigger();tray.showWindow();
        QTest::qWait(50);QVERIFY(window.isVisible());QVERIFY(control.controlSuspended());
    }
    void savedPreferencesAndVisibleRecoveryOverride(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;QQuickWindow window;window.show();
        {
            TrayController tray(&window,&control,settings,{},[]{return false;});
            action(tray,"trayStartHidden")->trigger();action(tray,"trayMinimize")->trigger();
            QVERIFY(settings.value("tray/startHidden").toBool());QVERIFY(settings.value("tray/minimize").toBool());
        }
        {
            TrayController tray(&window,&control,settings,{false,false,true},[]{return true;});
            QVERIFY(!tray.available());QVERIFY(!tray.startHidden());QVERIFY(!tray.minimizeToTray());
            QVERIFY(window.isVisible());
        }
        {
            QSignalSpy closed(qApp,&QGuiApplication::lastWindowClosed);
            TrayController tray(&window,&control,settings,{},[]{return true;});
            QSignalSpy quit(&tray,&TrayController::quitReady);
            QVERIFY(tray.startHidden());QVERIFY(tray.minimizeToTray());QVERIFY(!window.isVisible());
            QCOMPARE(closed.count(),0);
            action(tray,"trayQuit")->trigger();QVERIFY(!window.isVisible());
            QCOMPARE(quit.count(),1);
        }
    }
    void germanMenuScreenshot(){
        QTranslator german;QVERIFY(german.load(TRAY_TEST_TRANSLATIONS));
        QCoreApplication::installTranslator(&german);
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;control.registerOnDataFrame();
        QQuickWindow window;
        TrayController tray(&window,&control,settings,{},[]{return false;});
        QCOMPARE(action(tray,"traySuspend")->text(),QString::fromUtf8("Steuerung pausieren"));
        tray.menu()->ensurePolished();tray.menu()->show();QTest::qWait(30);
        const auto screenshot=qEnvironmentVariable("ATTENDANT_TRAY_SCREENSHOT");
        if(!screenshot.isEmpty())QVERIFY(tray.menu()->grab().save(screenshot));
        tray.menu()->hide();QCoreApplication::removeTranslator(&german);
    }
    void quitWaitsForAcceptedClose(){
        QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
        AttendantController control;ReturnPendingWindow window;window.show();
        TrayController tray(&window,&control,settings,{},[]{return true;});
        QSignalSpy quit(&tray,&TrayController::quitReady);
        tray.hideWindow();tray.requestQuit();
        QVERIFY(window.isVisible());QCOMPARE(quit.count(),0);
        window.returnConfirmed=true;tray.requestQuit();
        QVERIFY(!window.isVisible());QCOMPARE(quit.count(),1);
    }
};
QTEST_MAIN(TrayTest)
#include "attendant_tray_test.moc"
