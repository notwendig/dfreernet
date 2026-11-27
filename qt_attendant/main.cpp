#include "attendant_controller.hpp"
#include "shutdown_signals.hpp"
#include "tray_controller.hpp"
#include "app_icon.hpp"
#include "capture_playback.hpp"
#include <QCommandLineParser>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <memory>
#include <QJsonDocument>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
namespace {
bool legacyInstancePresent(const QString &interface)
{
    struct stat current{};
    if(::stat(QCoreApplication::applicationFilePath().toLocal8Bit().constData(),&current)!=0)return true;
    const auto processes=QDir("/proc").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
    for(const auto &entry:processes) {
        bool numeric=false;
        const auto pid=entry.fileName().toLongLong(&numeric);
        if(!numeric || pid==QCoreApplication::applicationPid() || entry.ownerId()!=::geteuid())continue;
        QFile name(entry.filePath()+"/comm");
        if(!name.open(QIODevice::ReadOnly) || name.readAll().trimmed()!="qt_attendant")continue;
        struct stat running{};
        if(::stat((entry.filePath()+"/exe").toLocal8Bit().constData(),&running)==0 &&
           running.st_dev==current.st_dev && running.st_ino==current.st_ino)continue; // QLockFile covers this build
        QFile command(entry.filePath()+"/cmdline");
        if(!command.open(QIODevice::ReadOnly))continue;
        const auto arguments=QString::fromLocal8Bit(command.readAll()).split(QChar(0),Qt::SkipEmptyParts);
        if(arguments.contains("--offline") || arguments.contains("--observe") || arguments.contains("--play-capture"))continue;
        QString other="can0";
        for(int i=1;i<arguments.size();++i) {
            if(arguments[i]=="--interface" && i+1<arguments.size())other=arguments[++i];
            else if(arguments[i].startsWith("--interface="))other=arguments[i].mid(12);
        }
        if(other==interface)return true;
    }
    return false;
}
}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setDesktopFileName("dfreernet-attendant");app.setApplicationName("qt_attendant");app.setOrganizationName("LocalRNet");app.setApplicationVersion(ATTENDANT_VERSION);
    app.setWindowIcon(attendant::applicationIcon());
    QCommandLineParser parser;parser.setApplicationDescription("Linux R-net attendant module: automatic startup on can0 and confirmed control.");parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"interface","Linux SocketCAN interface.","interface","can0"});
    QCommandLineOption legacyTx("can-tx","Compatibility option; CAN control is the default.");
    legacyTx.setFlags(QCommandLineOption::HiddenFromHelp);parser.addOption(legacyTx);
    parser.addOption({"registration-only","Only enroll and load configuration; disable joystick controls."});
    parser.addOption({"initial-enrollment","Explicit one-shot initial enrollment; later power cycles use registered startup."});
    QCommandLineOption legacyEnrollment("experimental-enrollment","Compatibility option; automatic startup is the default.");
    legacyEnrollment.setFlags(QCommandLineOption::HiddenFromHelp);parser.addOption(legacyEnrollment);
    parser.addOption({"enrollment-profile","Capture-specific module enrollment profile.","file"});
    parser.addOption({"identity-file","Eight-byte module identity file.","file"});
    parser.addOption({"offline","Use only the local simulation; disable CAN output."});
    parser.addOption({"observe","Open the Linux CAN observer (receive only)."});
    parser.addOption({"start-in-tray","Start hidden in the system tray, if available."});
    parser.addOption({"minimize-to-tray","Hide the window in the system tray when minimized."});
    parser.addOption({"no-tray","Keep the window visible and ignore saved tray preferences."});
    parser.addOption({"language","en_US or de_DE.","language"});
    parser.addOption({"software","Use the software scene graph (offline UI checks)."});
    parser.addOption({"screenshot","Write a local window screenshot.","file"});
    parser.addOption({"quit-after","Exit after 500..60000 milliseconds (UI checks).","milliseconds"});
    parser.addOption({"portrait","Use a phone-sized window for layout checks."});
    parser.addOption({"play-capture","Play a saved candump log offline.","file"});
    parser.process(app);
    const bool live = !parser.isSet("offline") && !parser.isSet("observe") && !parser.isSet("play-capture");
    if ((parser.isSet("offline") && parser.isSet("observe")) ||
        (live && (parser.isSet("screenshot") || parser.isSet("quit-after")))) {
        std::fprintf(stderr,"Use --offline for automated UI checks; select only one runtime mode.\n");
        return 2;
    }
    if(parser.isSet("software"))QQuickWindow::setSceneGraphBackend("software");
    QQuickStyle::setStyle("Basic");
    QTranslator german,qtGerman;if(!german.load(":/i18n/rnet_de_DE.qm")){std::fprintf(stderr,"Translations unavailable.\n");return 2;}
    if(!qtGerman.load("qtbase_de",QLibraryInfo::path(QLibraryInfo::TranslationsPath)))qWarning("Qt standard German translations unavailable");
    auto language=parser.value("language");if(language.isEmpty())language=QSettings().value("language",QLocale::system().name().startsWith("de")?"de_DE":"en_US").toString();
    if(language!="en_US"&&language!="de_DE"){std::fprintf(stderr,"Use --language en_US or de_DE.\n");return 2;}
    auto setLanguage=[&](const QString &code){app.removeTranslator(&german);app.removeTranslator(&qtGerman);QLocale::setDefault(QLocale(code));if(code=="de_DE"){app.installTranslator(&qtGerman);app.installTranslator(&german);}app.setApplicationDisplayName(QCoreApplication::translate("RNet","R-Net Control"));};
    setLanguage(language);
    if (parser.isSet("play-capture")) {
        const auto capture = attendant::readCapture(parser.value("play-capture"));
        if (!capture.error.isEmpty()) { qCritical().noquote() << capture.error; return 2; }
        QQmlApplicationEngine playback;
        playback.rootContext()->setContextProperty("captureEvents", capture.events);
        playback.rootContext()->setContextProperty("captureDuration", capture.duration);
        playback.load(QUrl("qrc:/qt/qml/Dfree/Attendant/qml/CapturePlayback.qml"));
        if (playback.rootObjects().isEmpty()) return 2;
        auto *window = qobject_cast<QQuickWindow *>(playback.rootObjects().first());
        if (parser.isSet("screenshot") && window)
            QTimer::singleShot(350, window, [&, window]{
                if (!window->grabWindow().save(parser.value("screenshot"))) app.exit(2);
            });
        if (parser.isSet("quit-after")) {
            bool ok = false;
            const int delay = parser.value("quit-after").toInt(&ok);
            if (!ok || delay < 500 || delay > 60000) return 2;
            QTimer::singleShot(delay, &app, &QCoreApplication::quit);
        }
        return app.exec();
    }
    std::unique_ptr<QLockFile> instance;
    attendant::AttendantController controller;
    controller.allowCanTx(live);
    controller.setCanInterface(parser.value("interface"));
    controller.setRegistrationOnly(parser.isSet("registration-only") || parser.isSet("observe"));
    if (live) {
        QString profilePath=parser.value("enrollment-profile");
        if (profilePath.isEmpty()) {
            const auto installed=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/enrollment.json";
            profilePath=QFileInfo::exists(installed)?installed:
                QCoreApplication::applicationDirPath()+"/../attendant-enrollment-tables.json";
        }
        QFile profileFile(profilePath);
        QJsonParseError parseError;
        if (!profileFile.open(QIODevice::ReadOnly) || profileFile.size()>1024*1024) {
            std::fprintf(stderr,"Cannot read private enrollment profile: %s\n",qPrintable(profilePath));
            return 2;
        }
        const auto document=QJsonDocument::fromJson(profileFile.readAll(),&parseError);
        if (parseError.error!=QJsonParseError::NoError || !document.isObject() ||
            !controller.setEnrollmentProfile(document.object())) {
            std::fprintf(stderr,"Invalid or incomplete enrollment profile: %s\n",qPrintable(profilePath));
            return 2;
        }
        if (parser.isSet("identity-file")) {
            QFile identity(parser.value("identity-file"));
            if (!identity.open(QIODevice::ReadOnly) || identity.size()!=8 ||
                identity.readAll()!=QByteArray::fromHex(document.object().value("identity").toString().toLatin1())) {
                std::fprintf(stderr,"Identity file does not match enrollment profile.\n");
                return 2;
            }
        }
        auto runtime=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if(runtime.isEmpty())runtime=QDir::tempPath();
        const auto key=QString::fromLatin1(QCryptographicHash::hash(
            (QString::number(::geteuid())+":"+parser.value("interface")).toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
        instance=std::make_unique<QLockFile>(runtime+"/rnet-attendant-"+key+".lock");
        instance->setStaleLockTime(0);
        if (!instance->tryLock(0)) {
            std::fprintf(stderr,"An attendant module already owns this interface.\n");
            return 2;
        }
        if (legacyInstancePresent(parser.value("interface"))) {
            std::fprintf(stderr,"A previous attendant build is still running; close it before restarting.\n");
            return 2;
        }
        if (parser.isSet("initial-enrollment")) controller.requestInitialEnrollment();
        auto lastStatus=std::make_shared<QString>();
        QObject::connect(&controller,&attendant::AttendantController::changed,&app,[&,lastStatus]{
            const auto status=controller.statusText()+" | "+controller.detail();
            if(status!=*lastStatus) {
                *lastStatus=status;
                std::fprintf(stderr,"BGM %s TX %s\n",qPrintable(status),qPrintable(controller.canTxCount()));
                std::fflush(stderr);
            }
        });
    }
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("controller",&controller);
    QSettings traySettings;
    std::unique_ptr<attendant::TrayController> tray;
    QObject::connect(&controller,&attendant::AttendantController::languageRequested,&engine,[&](const QString &code){if(code!="en_US"&&code!="de_DE")return;setLanguage(code);QSettings().setValue("language",code);engine.retranslate();if(tray)tray->retranslate();});
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&controller,&attendant::AttendantController::shutdown);
    QObject::connect(&controller,&attendant::AttendantController::closeReady,&app,&QCoreApplication::quit,Qt::QueuedConnection);
    engine.load(QUrl("qrc:/qt/qml/Dfree/Attendant/qml/Main.qml"));if(engine.rootObjects().isEmpty())return 2;
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window)return 2;
    window->setIcon(app.windowIcon());
    QObject::connect(&app,&QGuiApplication::applicationStateChanged,&controller,[&](Qt::ApplicationState state){controller.setApplicationActive(state==Qt::ApplicationActive && window->isVisible() && window->isActive());});
    QObject::connect(window,&QWindow::activeChanged,&controller,[&]{controller.setApplicationActive(window->isActive() && window->isVisible());});
    QObject::connect(window,&QWindow::visibleChanged,&controller,[&](bool visible){if(!visible)controller.setApplicationActive(false);});
    // Diagnostics and automated screenshots always use a visible window.
    if(live || parser.isSet("start-in-tray") || parser.isSet("minimize-to-tray")) {
        tray=std::make_unique<attendant::TrayController>(window,&controller,traySettings,
            attendant::TrayController::Options{parser.isSet("start-in-tray"),parser.isSet("minimize-to-tray"),parser.isSet("no-tray") || parser.isSet("screenshot")});
        QObject::connect(tray.get(),&attendant::TrayController::quitReady,&app,&QCoreApplication::quit);
    }
    attendant::ShutdownSignals shutdownSignals;
    const QPointer<QQuickWindow> guardedWindow(window);
    if(!shutdownSignals.install([guardedWindow,&tray]{if(tray)tray->requestQuit();else if(guardedWindow)guardedWindow->close();})) {
        std::fprintf(stderr,"Cannot install orderly shutdown handlers.\n");
        return 2;
    }
    if(parser.isSet("portrait")){window->setWidth(420);window->setHeight(900);}
    if(parser.isSet("offline")) controller.registerOnDataFrame();
    else QTimer::singleShot(0, &controller, &attendant::AttendantController::observeCan);
    if(parser.isSet("screenshot"))QTimer::singleShot(350,window,[&]{if(!window->grabWindow().save(parser.value("screenshot"))){std::fprintf(stderr,"Screenshot failed.\n");app.exit(2);}});
    if(parser.isSet("quit-after")){bool ok=false;int ms=parser.value("quit-after").toInt(&ok);if(!ok||ms<500||ms>60000){std::fprintf(stderr,"Use --quit-after 500..60000.\n");return 2;}QTimer::singleShot(ms,&app,&QCoreApplication::quit);}
    return app.exec();
}
