#pragma once
#include <QObject>
#include <QString>
#include <QTranslator>
#include <stdexcept>
namespace rnet::qt {
class UiError : public std::runtime_error {
public:
  explicit UiError(const QString &message)
      : std::runtime_error(message.toStdString()) {}
  explicit UiError(const std::string &message) : std::runtime_error(message) {}
  explicit UiError(const char *message) : std::runtime_error(message) {}
};
class Languages final : public QObject {
public:
  explicit Languages(QObject *parent);
  const QString &language() const { return language_; }
  void setLanguage(const QString &code, bool persist = false);
  QString savedLanguage() const;

private:
  QString language_;
  QTranslator german_, qtGerman_;
};
Languages &languages();
QString localizedError(const char *message);
} // namespace rnet::qt
