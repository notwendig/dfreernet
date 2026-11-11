#pragma once
#include "values.hpp"
#include <QWidget>
#include <functional>
class QLineEdit;
class QComboBox;
namespace rnet::qt {
class ParameterValueEditor final : public QWidget {
  const ParameterCatalog &catalog_;
  ParameterDefinition definition_;
  DocumentValue value_;
  QLineEdit *line_ = nullptr;
  QComboBox *combo_ = nullptr;
  qint64 minimum_=0, maximum_=0, step_=1;
  void refresh();
  void step(int direction);
public:
  ParameterValueEditor(const ParameterDefinition&, const DocumentValue&,
                       const ParameterCatalog&, QWidget *parent=nullptr);
  QString rawValue() const;
  bool valid(QString *error=nullptr) const;
  std::function<void()> validityChanged;
};
}
