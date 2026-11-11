#pragma once
#include <QDialog>
namespace rnet::qt {
// Shown on every interactive launch before constructing the controller UI.
class StartupDialog final : public QDialog {
public:
  explicit StartupDialog(QWidget *parent=nullptr);
  void placeOnSecondary();
protected:
  void showEvent(QShowEvent*) override;
};
}
