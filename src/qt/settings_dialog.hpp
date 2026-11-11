#pragma once
#include "app_settings.hpp"
#include <QDialog>
#include <QPointer>
#include <functional>
#include <vector>
class QListWidget;class QStackedWidget;class QComboBox;class QSpinBox;class QCheckBox;class QLineEdit;class QLabel;class QFormLayout;
namespace rnet::qt {
class SettingsDialog final : public QDialog {
  AppSettings settings_;
  QListWidget *tabs_=nullptr;QStackedWidget *pages_=nullptr;
  QComboBox *language_,*access_,*interface_,*monitor_;
  QSpinBox *duration_,*undo_,*toolbarIcons_,*itemIcons_,*font_,*heartbeatTimeout_,*liveInterval_,*liveDuration_;
  QCheckBox *maximized_,*details_,*heartbeat_,*keepAwake_,*showAll_,*liveSignal_,*toolbarIconsOnly_;
  QLineEdit *working_,*backup_,*exports_,*logs_;
  QLabel *error_;
  struct Binding{QPointer<QObject> object;QByteArray property,source;};
  std::vector<Binding> texts_;
  void bind(QObject*,const char*,const char*);
  QFormLayout *page(const char*);
  QLineEdit *directory(QFormLayout*,const char*,const char*,const QString&);
  void retranslate();
  bool apply();
public:
  explicit SettingsDialog(const AppSettings&,QWidget *parent=nullptr);
  std::function<void(const AppSettings&)> applied;
protected:
  void changeEvent(QEvent*) override;
};
}
