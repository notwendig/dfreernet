#pragma once

#include <QElapsedTimer>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <functional>
#include <map>

namespace rnet::qt {

class CanFrameTable final : public QTableWidget {
public:
  enum class AverageUnit { Hertz, Milliseconds };
  explicit CanFrameTable(QWidget *parent, const char *name, const QFont &font);
  ~CanFrameTable() override;
  void observe(const QString &id, unsigned dlc, const QString &payload);
  void resetCounts();
  void refresh();
  void retranslate();
  void setAverageUnit(AverageUnit unit);

private:
  struct Count {
    quint64 frames = 0;
    unsigned dlc = 0;
    QString payload;
  };
  std::map<QString, Count> counts_;
  QElapsedTimer elapsed_;
  QString layoutKey_;
  QTimer *layoutSaveTimer_ = nullptr;
  bool layoutDirty_ = false;
  AverageUnit averageUnit_ = AverageUnit::Hertz;
  void updateAverages();
  void saveLayout();
};

class CanViewControls final : public QWidget {
public:
  CanViewControls(QWidget *parent, const char *name, const QString &settingsPrefix,
                  bool defaultFollow, std::function<void()> changed);
  bool follow() const;
  CanFrameTable::AverageUnit averageUnit() const;
  void retranslate();

private:
  QToolButton *follow_ = nullptr;
  QToolButton *count_ = nullptr;
  QToolButton *hertz_ = nullptr;
  QToolButton *milliseconds_ = nullptr;
};

} // namespace rnet::qt
