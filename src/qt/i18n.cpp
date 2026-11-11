#include "i18n.hpp"
#include "translation_keys.hpp"
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
static void initializeTranslations() { Q_INIT_RESOURCE(rnet_translations); }
namespace rnet::qt {
Languages::Languages(QObject *parent) : QObject(parent) {
  initializeTranslations();
  if (!german_.load(":/i18n/rnet_de_DE.qm"))
    throw UiError("German translation resource is missing");
  if (!qtGerman_.load("qtbase_de",
                      QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    qWarning("Qt German standard-dialog translations are unavailable");
  setLanguage("en_US");
}
QString Languages::savedLanguage() const {
  auto saved = QSettings().value("ui/language", "en_US").toString();
  return saved == "de_DE" ? saved : QString("en_US");
}
void Languages::setLanguage(const QString &code, bool persist) {
  if (code != "en_US" && code != "de_DE")
    throw UiError(QCoreApplication::translate("RNet", "Unsupported language"));
  if (code != language_) {
    language_ = code;
    if (code == "de_DE") {
      QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
      QCoreApplication::installTranslator(&qtGerman_);
      QCoreApplication::installTranslator(&german_);
    } else {
      QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
      QCoreApplication::removeTranslator(&german_);
      QCoreApplication::removeTranslator(&qtGerman_);
    }
  }
  if (persist) {
    QSettings settings;
    settings.setValue("ui/language", code);
    settings.sync();
  }
}
Languages &languages() {
  static QPointer<Languages> instance;
  if (!instance)
    instance = new Languages(QCoreApplication::instance());
  return *instance;
}
QString localizedError(const char *message) {
  QString source = QString::fromUtf8(message);
  auto match =
      QRegularExpression("^Value must be between (.*) and (.*)$").match(source);
  if (match.hasMatch())
    return QCoreApplication::translate("RNet",
                                       "Value must be between %1 and %2")
        .arg(match.captured(1), match.captured(2));
  return QCoreApplication::translate("RNet", message);
}
} // namespace rnet::qt
