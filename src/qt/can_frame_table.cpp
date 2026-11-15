#include "can_frame_table.hpp"
#include "icons.hpp"
#include <QButtonGroup>
#include <QCoreApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QSettings>
#include <QTimer>

namespace rnet::qt {
namespace {
class SortItem final : public QTableWidgetItem {
public:
  enum class Kind { Text, Integer, Rate };
  explicit SortItem(Kind kind) : kind_(kind) {}
  bool operator<(const QTableWidgetItem &other) const override {
    const auto *rhs = dynamic_cast<const SortItem *>(&other);
    if (rhs && rhs->kind_ == kind_) {
      if (kind_ == Kind::Integer) {
        const auto left = data(Qt::UserRole).toULongLong();
        const auto right = rhs->data(Qt::UserRole).toULongLong();
        if (left != right) return left < right;
      } else if (kind_ == Kind::Rate) {
        const auto left = data(Qt::UserRole).toDouble();
        const auto right = rhs->data(Qt::UserRole).toDouble();
        if (left != right) return left < right;
      }
    }
    return QTableWidgetItem::operator<(other);
  }
private:
  Kind kind_;
};
}

CanFrameTable::CanFrameTable(QWidget *parent, const char *name, const QFont &font)
    : QTableWidget(parent) {
  setObjectName(name);
  setColumnCount(4);
  retranslate();
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setSelectionMode(QAbstractItemView::NoSelection);
  setFocusPolicy(Qt::NoFocus);
  verticalHeader()->hide();
  horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  horizontalHeader()->setSectionsClickable(true);
  horizontalHeader()->setSectionsMovable(true);
  horizontalHeader()->setSortIndicatorShown(true);
  horizontalHeader()->setStretchLastSection(false);
  setColumnWidth(0, 105);
  setColumnWidth(1, 115);
  setColumnWidth(2, 90);
  setColumnWidth(3, 280);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setFont(font);
  layoutKey_ = QStringLiteral("ui/columns/v1/") + QString::fromLatin1(name);
  sortKey_ = QStringLiteral("ui/canSort/v1/") + QString::fromLatin1(name);
  const auto layout = QSettings().value(layoutKey_).toByteArray();
  if (!layout.isEmpty()) horizontalHeader()->restoreState(layout);
  const auto savedSort = QSettings().value(sortKey_).toList();
  const int sortColumn = savedSort.size() == 2 && savedSort[0].toInt() >= 0 &&
                                 savedSort[0].toInt() < columnCount()
                             ? savedSort[0].toInt() : 1;
  const auto sortOrder = savedSort.size() == 2 && savedSort[1].toInt() == int(Qt::DescendingOrder)
                             ? Qt::DescendingOrder : Qt::AscendingOrder;
  setSortingEnabled(true);
  sortItems(sortColumn, sortOrder);
  layoutSaveTimer_ = new QTimer(this);
  layoutSaveTimer_->setSingleShot(true);
  layoutSaveTimer_->setInterval(250);
  connect(layoutSaveTimer_, &QTimer::timeout, this, [this] { saveLayout(); });
  auto changed = [this] { layoutDirty_ = true; layoutSaveTimer_->start(); };
  connect(horizontalHeader(), &QHeaderView::sectionResized, this,
          [changed](int, int, int) { changed(); });
  connect(horizontalHeader(), &QHeaderView::sectionMoved, this,
          [changed](int, int, int) { changed(); });
  connect(horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
          [changed](int, Qt::SortOrder) { changed(); });
  elapsed_.start();
  auto *timer = new QTimer(this);
  timer->setInterval(1000);
  connect(timer, &QTimer::timeout, this, [this] { updateAverages(); });
  timer->start();
}

CanFrameTable::~CanFrameTable() { saveLayout(); }

void CanFrameTable::saveLayout() {
  if (!layoutDirty_) return;
  QSettings settings;
  settings.setValue(layoutKey_, horizontalHeader()->saveState());
  settings.setValue(sortKey_, QVariantList{horizontalHeader()->sortIndicatorSection(),
                                          int(horizontalHeader()->sortIndicatorOrder())});
  settings.sync();
  if (settings.status() == QSettings::NoError) layoutDirty_ = false;
}

void CanFrameTable::retranslate() {
  setHorizontalHeaderLabels({QCoreApplication::translate("RNet", "Count"),
                             QStringLiteral("CAN ID"),
                             averageUnit_ == AverageUnit::Hertz ? QStringLiteral("avg [Hz]")
                                                                 : QStringLiteral("avg [ms]"),
                             QCoreApplication::translate("RNet", "Latest payload")});
}

void CanFrameTable::setAverageUnit(AverageUnit unit) {
  if (averageUnit_ == unit) return;
  averageUnit_ = unit;
  retranslate();
  updateAverages();
}

void CanFrameTable::observe(const QString &id, unsigned dlc, const QString &payload) {
  auto &count = counts_[id];
  ++count.frames;
  count.dlc = dlc;
  count.payload = payload;
}

void CanFrameTable::resetCounts() {
  counts_.clear();
  setRowCount(0);
  elapsed_.restart();
}

void CanFrameTable::refresh() {
  const int sortColumn = horizontalHeader()->sortIndicatorSection();
  const auto sortOrder = horizontalHeader()->sortIndicatorOrder();
  setSortingEnabled(false);
  setRowCount(int(counts_.size()));
  int row = 0;
  for (const auto &[id, count] : counts_) {
    auto setCell = [this, row](int column, const QString &value, SortItem::Kind kind,
                               const QVariant &sortValue = {}, bool right = false) {
      auto *item = this->item(row, column);
      if (!item) {
        item = new SortItem(kind);
        setItem(row, column, item);
      }
      item->setText(value);
      if (sortValue.isValid()) item->setData(Qt::UserRole, sortValue);
      if (right) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    };
    setCell(0, QString::number(count.frames).rightJustified(8, ' '),
            SortItem::Kind::Integer, count.frames, true);
    setCell(1, id, SortItem::Kind::Integer, id.section(' ', 0, 0).toULongLong(nullptr, 16), true);
    setCell(3, QStringLiteral("[%1] %2").arg(count.dlc).arg(count.payload),
            SortItem::Kind::Text);
    ++row;
  }
  updateAverages();
  setSortingEnabled(true);
  sortItems(sortColumn, sortOrder);
}

void CanFrameTable::updateAverages() {
  if (!isVisible()) return;
  const bool sorting = isSortingEnabled();
  const int sortColumn = horizontalHeader()->sortIndicatorSection();
  const auto sortOrder = horizontalHeader()->sortIndicatorOrder();
  if (sorting) setSortingEnabled(false);
  const double seconds = qMax(1.0, elapsed_.elapsed() / 1000.0);
  for (int row = 0; row < rowCount(); ++row) {
    const auto idItem = this->item(row, 1);
    if (!idItem) continue;
    const auto found = counts_.find(idItem->text());
    if (found == counts_.end()) continue;
    const auto &count = found->second;
    auto *item = this->item(row, 2);
    if (!item) {
      item = new SortItem(SortItem::Kind::Rate);
      setItem(row, 2, item);
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    const double value = averageUnit_ == AverageUnit::Hertz
                             ? count.frames / seconds
                             : seconds * 1000.0 / count.frames;
    item->setText(QString::number(value, 'f', 2));
    item->setData(Qt::UserRole, value);
  }
  if (sorting) {
    setSortingEnabled(true);
    sortItems(sortColumn, sortOrder);
  }
}

CanViewControls::CanViewControls(QWidget *parent, const char *name,
                                 const QString &settingsPrefix, bool defaultFollow,
                                 std::function<void()> changed)
    : QWidget(parent) {
  setObjectName(name);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);
  const auto prefix = QString::fromLatin1(name);
  auto button = [&](const QString &suffix, Icon icon) {
    auto *result = new QToolButton(this);
    result->setObjectName(prefix + suffix);
    result->setCheckable(true);
    result->setAutoRaise(true);
    result->setIcon(uiIcon(icon));
    result->setIconSize(QSize(20, 20));
    result->setFixedSize(32, 30);
    result->setToolButtonStyle(Qt::ToolButtonIconOnly);
    result->setStyleSheet("QToolButton:checked { background: palette(midlight); "
                          "border: 1px solid palette(mid); border-radius: 4px; }");
    layout->addWidget(result);
    return result;
  };
  follow_ = button("Follow", Icon::Monitor);
  count_ = button("Count", Icon::Number);
  layout->addSpacing(8);
  hertz_ = button("Hz", Icon::Signal);
  milliseconds_ = button("Ms", Icon::Clock);
  layout->addStretch();
  auto *viewGroup = new QButtonGroup(this);
  viewGroup->addButton(follow_);
  viewGroup->addButton(count_);
  auto *unitGroup = new QButtonGroup(this);
  unitGroup->addButton(hertz_);
  unitGroup->addButton(milliseconds_);
  const QSettings settings;
  const int defaultMode = defaultFollow ? 0 : 1;
  (settings.value(settingsPrefix + "Mode", defaultMode).toInt() == 0 ? follow_ : count_)
      ->setChecked(true);
  (settings.value(settingsPrefix + "Unit", 0).toInt() == 1 ? milliseconds_ : hertz_)
      ->setChecked(true);
  connect(follow_, &QToolButton::clicked, this, [settingsPrefix, changed] {
    QSettings().setValue(settingsPrefix + "Mode", 0);
    changed();
  });
  connect(count_, &QToolButton::clicked, this, [settingsPrefix, changed] {
    QSettings().setValue(settingsPrefix + "Mode", 1);
    changed();
  });
  connect(hertz_, &QToolButton::clicked, this, [settingsPrefix, changed] {
    QSettings().setValue(settingsPrefix + "Unit", 0);
    changed();
  });
  connect(milliseconds_, &QToolButton::clicked, this, [settingsPrefix, changed] {
    QSettings().setValue(settingsPrefix + "Unit", 1);
    changed();
  });
  retranslate();
}

bool CanViewControls::follow() const { return follow_->isChecked(); }

CanFrameTable::AverageUnit CanViewControls::averageUnit() const {
  return hertz_->isChecked() ? CanFrameTable::AverageUnit::Hertz
                            : CanFrameTable::AverageUnit::Milliseconds;
}

void CanViewControls::retranslate() {
  auto label = [](QToolButton *button, const char *source) {
    const auto translated = QCoreApplication::translate("RNet", source);
    button->setToolTip(translated);
    button->setAccessibleName(translated);
  };
  label(follow_, "Follow");
  label(count_, "Count");
  label(hertz_, "Average rate (Hz)");
  label(milliseconds_, "Average interval (ms)");
}

} // namespace rnet::qt
