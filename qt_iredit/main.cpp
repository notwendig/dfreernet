#include "editor_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <cstdio>
int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setApplicationName("qt_iredit"); app.setOrganizationName("LocalRNet"); app.setApplicationVersion(QT_IREDIT_VERSION);
    QCommandLineParser parser; parser.setApplicationDescription("Offline Qt editor for R-net IR menus and Pronto Hex codes."); parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("file", "RnIR3 menu or IR editor JSON project to open.", "[file]");
    parser.addOption({"language", "Interface language: en_US or de_DE.", "code"});
    parser.addOption({"export-rnir3", "Convert the input document to a local RnIR3 file and exit.", "file"});
    parser.addOption({"screenshot", "Save an editor screenshot (UI checks).", "file"});
    parser.addOption({"quit-after", "Close after this many milliseconds (UI checks).", "milliseconds"});
    parser.process(app);
    auto files = parser.positionalArguments(); if (files.size() > 1) parser.showHelp(2);
    QTranslator german, qtGerman;
    if (!german.load(":/i18n/rnet_de_DE.qm")) { std::fprintf(stderr, "German translations unavailable.\n"); return 2; }
    if (!qtGerman.load("qtbase_de", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) qWarning("Qt German standard-dialog translations unavailable.");
    QString language = parser.value("language"); if (language.isEmpty()) language = QSettings().value("language", "en_US").toString();
    if (language != "en_US" && language != "de_DE") { std::fprintf(stderr, "Use --language en_US or de_DE.\n"); return 2; }
    auto setLanguage = [&](const QString &code) {
        app.removeTranslator(&german); app.removeTranslator(&qtGerman);
        QLocale::setDefault(code == "de_DE" ? QLocale(QLocale::German, QLocale::Germany) : QLocale(QLocale::English, QLocale::UnitedStates));
        if (code == "de_DE") { app.installTranslator(&qtGerman); app.installTranslator(&german); }
    };
    setLanguage(language);
    try {
        if (parser.isSet("export-rnir3")) {
            if (files.size() != 1 || QFileInfo(parser.value("export-rnir3")).suffix().compare("RnIR3", Qt::CaseInsensitive) != 0) parser.showHelp(2);
            iredit::Document::load(files[0]).save(parser.value("export-rnir3")); return 0;
        }
        iredit::EditorWindow window;
        window.changeLanguage = [&](const QString &code) { setLanguage(code); QSettings().setValue("language", code); window.retranslate(); };
        if (!files.isEmpty()) window.openFile(files[0]);
        window.show();
        if (parser.isSet("screenshot")) QTimer::singleShot(250, &window, [&] {
            if (!window.grab().save(parser.value("screenshot"))) { std::fprintf(stderr, "Could not save screenshot.\n"); app.exit(2); }
        });
        if (parser.isSet("quit-after")) {
            bool ok = false; auto ms = parser.value("quit-after").toInt(&ok);
            if (!ok || ms < 300 || ms > 60000) { std::fprintf(stderr, "Use --quit-after 300..60000.\n"); return 2; }
            QTimer::singleShot(ms, &app, &QApplication::quit);
        }
        return app.exec();
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 2; }
}
