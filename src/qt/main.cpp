#include "i18n.hpp"
#include "window.hpp"
#include "startup_dialog.hpp"
#include "app_settings.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include <cstdio>
using rnet::qt::UiError;
int main(int argc, char **argv) {
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("dfreernet");
  QCoreApplication::setOrganizationName("LocalRNet");
  QCoreApplication::setApplicationVersion("0.1.0");
  QGuiApplication::setApplicationDisplayName("dfreernet");
  // Keep existing user preferences when upgrading from the former Qt app name.
  QSettings settings;
  if (settings.allKeys().isEmpty()) {
    QSettings previous("LocalRNet", "R-net Qt");
    for (const auto &key : previous.allKeys()) settings.setValue(key, previous.value(key));
    settings.sync();
  }
  QCommandLineParser parser;
  parser.setApplicationDescription(QCoreApplication::translate(
      "RNet", "dfreernet — Dongle free R-Net Programmer"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption legacyInstallation("installation", "Ignored legacy option", "directory");
  legacyInstallation.setFlags(QCommandLineOption::HiddenFromHelp);
  parser.addOption(legacyInstallation);
  parser.addOption({"document",
                    QCoreApplication::translate("RNet", "Open R-net document"),
                    "file"});
  parser.addOption(
      {"output", QCoreApplication::translate("RNet", "Log directory"),
       "directory",
       QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           "/logs"});
  parser.addOption({"screenshot",
                    QCoreApplication::translate(
                        "RNet", "Save a screenshot of the open window"),
                    "file"});
  parser.addOption(
      {"quit-after",
       QCoreApplication::translate(
           "RNet", "Close after the specified milliseconds (UI checks only)"),
       "milliseconds"});
  parser.addOption(
      {"read-controller",
       QCoreApplication::translate(
           "RNet", "Read the controller over SocketCAN (read only)"),
       "interface"});
  parser.addOption({"backup-directory",
                    QCoreApplication::translate(
                        "RNet", "New folder for the configuration read"),
                    "directory"});
  parser.addOption({"hazard-blink-test",QCoreApplication::translate("RNet","Start the configured hazard-light cycle")});
  parser.addOption({"stationary-confirmed",
                    QCoreApplication::translate(
                        "RNet", "Stationary operation and no need for driving "
                                "explicitly confirmed")});
  parser.addOption(
      {"language",
       QCoreApplication::translate("RNet", "UI language (en_US or de_DE)"),
       "code"});
  parser.process(app);
  try {
    const auto preferences=rnet::qt::AppSettings::load();
    rnet::qt::applyAppFont(preferences.fontSize);
    auto &language = rnet::qt::languages();
    language.setLanguage(parser.isSet("language") ? parser.value("language")
                                                  : language.savedLanguage());
    int quitAfter=0;
    if(parser.isSet("quit-after")){
      bool valid=false;quitAfter=parser.value("quit-after").toInt(&valid);
      if(!valid||quitAfter<1)throw UiError(QCoreApplication::translate("RNet","Invalid close timeout"));
    }
    // No document opening, controller UI or automatic CAN task before consent.
    app.setQuitOnLastWindowClosed(false);
    {
      rnet::qt::StartupDialog notice;
      notice.placeOnSecondary();
      if(parser.isSet("screenshot"))QTimer::singleShot(1000,&notice,[&]{notice.grab().save(parser.value("screenshot"));});
      // UI-check timeout may reject the notice, but can never accept it.
      if(quitAfter)QTimer::singleShot(quitAfter,&notice,&QDialog::reject);
      if(notice.exec()!=QDialog::Accepted)return 0;
    }
    app.setQuitOnLastWindowClosed(true);
    rnet::qt::Window window(parser.isSet("output")?parser.value("output"):preferences.logDirectory);
    if (parser.isSet("document"))
      window.openDocument(parser.value("document"));
    window.placeOnSecondary();
    if(parser.isSet("hazard-blink-test")&&parser.isSet("read-controller"))throw UiError(QCoreApplication::translate("RNet","The light cycle cannot start together with a controller read"));
    if (parser.isSet("read-controller")) {
      if (!parser.isSet("stationary-confirmed") ||
          !parser.isSet("backup-directory"))
        throw UiError(QCoreApplication::translate(
            "RNet", "Reading the controller requires confirmed stationary "
                    "operation and a new backup folder"));
      QTimer::singleShot(0, &window, [&] {
        try {
          window.startControllerRead(parser.value("read-controller"),
                                     parser.value("backup-directory"), true);
        } catch (const std::exception &error) {
          QMessageBox::warning(
              &window, QCoreApplication::translate("RNet", "Read controller"),
              rnet::qt::localizedError(error.what()));
        }
      });
    }
    if((parser.isSet("hazard-blink-test")||preferences.liveSignalEnabled)&&!parser.isSet("read-controller"))
      QTimer::singleShot(0,&window,[&window]{window.requestBlinkCycle();});
    if (parser.isSet("screenshot"))
      QTimer::singleShot(1000, &window, [&] {
        window.grab().save(parser.value("screenshot"));
      });
    if(quitAfter)QTimer::singleShot(quitAfter,&window,&QWidget::close);
    return app.exec();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "dfreernet: %s\n", error.what());
    QMessageBox::critical(
        nullptr, QCoreApplication::translate("RNet", "dfreernet could not start"),
        rnet::qt::localizedError(error.what()));
    return 1;
  }
}
