#pragma once
#include "database.hpp"
#include "document.hpp"
#include "pe_image.hpp"
#include "values.hpp"
#include "icons.hpp"
#include "app_settings.hpp"
#include <QAbstractTableModel>
#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QSortFilterProxyModel>
#include <functional>
#include <memory>
class QTreeWidget;
class QTreeWidgetItem;
class QTableView;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QAction;
class QScreen;
class QMenu;
class QSocketNotifier;
class QTimer;
namespace rnet::qt {
class CanHeartbeatIndicator;
class SleepInhibitor;
class HazardBlinkCycle;
struct Row {
  QString id, name, value, location, group, detail;
  int valueIndex = -1;
  int requiredAccess = -1;
  bool changed = false;
  Icon icon = Icon::None;
  bool unread = false;
  struct Cell {
    QString value, detail;
    int valueIndex = -1;
    bool changed = false;
  };
  std::vector<Cell> cells{};
  QString module{};
  QString unit{},scale{},minimum{},maximum{},metadataSummary{};
};
class TableModel final : public QAbstractTableModel {
public:
  std::vector<Row> rows;
  bool profileLayout = false;
  QStringList headings{"Parameter ID", "Parameter", "Stored raw value",
                       "Profile / element"};
  using QAbstractTableModel::QAbstractTableModel;
  int rowCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : int(rows.size());
  }
  int columnCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : headings.size();
  }
  QVariant data(const QModelIndex &, int role = Qt::DisplayRole) const override;
  QVariant headerData(int, Qt::Orientation,
                      int role = Qt::DisplayRole) const override;
  std::function<bool(int)> canEdit;
  Qt::ItemFlags flags(const QModelIndex &index) const override {
    auto result=QAbstractTableModel::flags(index);
    int valueIndex=-1;
    if(index.isValid()&&index.row()<int(rows.size())){
      const auto &row=rows.at(index.row());
      if(profileLayout&&index.column()>=2&&index.column()<headings.size()-1)
        valueIndex=row.cells.at(std::size_t(index.column()-2)).valueIndex;
      else if(!profileLayout&&index.column()==2)valueIndex=row.valueIndex;
    }
    if(canEdit&&canEdit(valueIndex))
      result|=Qt::ItemIsEditable;
    return result;
  }
  void replace(std::vector<Row>, QStringList, bool profiles=false);
};
class Filter final : public QSortFilterProxyModel {
public:
  QString query, group;
  AccessLevel access = AccessLevel::Dealer;
  using QSortFilterProxyModel::QSortFilterProxyModel;
  void refilter();

protected:
  bool filterAcceptsRow(int, const QModelIndex &) const override;
};
class Window final : public QMainWindow {
  friend class WindowTest;

public:
  explicit Window(const QString &output,
                  QWidget *parent = nullptr);
  ~Window() override;
  void openDocument(const QString &);
  void saveAs(const QString &);
  void save();
  void applyEdit(int, const QString &);
  void undo();
  void setAccess(AccessLevel);
  void showValues();
  void showCatalog();
  void showRepository();
  void showResources();
  void placeOnSecondary();
  void startBlinkCycle();
  void requestBlinkCycle();
  bool heartbeatReceiving()const;
  int valueCount() const { return int(values_.size()); }
  int warningCount() const { return int(warnings_.size()); }
  int fileCount() const { return document_ ? int(document_->files.size()) : 0; }
  const RnetDocument *document() const { return document_.get(); }
  AccessLevel access() const { return access_; }
  QString documentPath() const { return documentPath_; }
  TableModel *model() const { return model_; }
  Filter *filter() const { return proxy_; }
  void exportCsv(const QString &);
  void startMonitor(const QString &interface, int milliseconds = 10000);
  void startControllerRead(const QString &interface, const QString &destination,
                           bool stationaryConfirmed);
  void startControllerWrite(const QString &, const QString &, bool stationaryConfirmed);
  bool monitoring() const { return monitorFd_ >= 0; }
  quint64 receivedFrames() const { return frameCount_; }

protected:
  void closeEvent(QCloseEvent *) override;
  void changeEvent(QEvent *) override;
  bool eventFilter(QObject *,QEvent *) override;

private:
  ParameterCatalog catalog_;
  QString output_, documentPath_;
  bool owned_ = false, valuesView_ = false;
  enum class View { Values, Catalog, Repository, Resources, Monitor };
  View view_ = View::Values;
  bool uiReady_ = false;
  struct TextBinding {
    QPointer<QObject> object;
    QByteArray property;
    QByteArray source;
  };
  std::vector<TextBinding> textBindings_;
  std::vector<QAction *> languageActions_;
  std::unique_ptr<RnetDocument> document_;
  Bytes saved_, controllerBaseline_;
  std::vector<DocumentValue> values_;
  std::vector<std::string> warnings_;
  std::vector<Bytes> undo_;
  AppSettings settings_;
  HazardBlinkCycle *blinkCycle_=nullptr;
  QAction *blinkAction_=nullptr;
  QLabel *blinkLabel_=nullptr,*accessLabel_=nullptr;

  void updateBlinkState();
  SleepInhibitor *sleepInhibitor_=nullptr;
  QLabel *sleepLabel_=nullptr;
  QString sleepStatus_;
  CanHeartbeatIndicator *heartbeat_=nullptr;
  QAction *detailsAction_=nullptr;
  QAction *parameterHelpAction_=nullptr;
  QAction *listViewAction_=nullptr,*profileViewAction_=nullptr,*moduleViewAction_=nullptr;
  QAction *saveColumnsAction_=nullptr,*loadColumnsAction_=nullptr,*resetColumnsAction_=nullptr;
  AccessLevel access_ = AccessLevel::Dealer;
  TableModel *model_ = nullptr;
  Filter *proxy_ = nullptr;
  QTreeWidget *tree_ = nullptr;
  QTableView *table_ = nullptr,*accessTable_=nullptr;
  QTimer *columnSaveTimer_=nullptr;
  bool columnLayoutBusy_=false,columnLayoutReady_=false;
  QString pendingColumnKey_;
  QByteArray pendingColumnState_;
  QLineEdit *search_ = nullptr;
  QPlainTextEdit *details_ = nullptr;
  QLabel *banner_ = nullptr;
  QPushButton *edit_ = nullptr;
  QPushButton *undoButton_ = nullptr;
  std::map<unsigned, QAction *> actions_;
  std::vector<QAction *> roleActions_;
  QProcess *process_ = nullptr;
  QString processText_, taskDirectory_;
  bool configTask_ = false, writeTask_ = false, cancelRequested_ = false;
  QPlainTextEdit *taskLog_ = nullptr;
  QPushButton *cancel_ = nullptr;
  QWidget *taskPanel_ = nullptr;
  int monitorFd_ = -1;
  QSocketNotifier *notifier_ = nullptr;
  QTimer *monitorTimer_ = nullptr;
  quint64 frameCount_ = 0;
  struct Observed {
    quint64 count = 0;
    QByteArray data;
  };
  std::map<unsigned, Observed> observed_;
  void log(const QString &);
  void guarded(const std::function<void()> &);
  void updateActions();
  void updateTitle();
  QMenu *makeMenu(const std::vector<MenuItem> &,
                          QMenu *parent = nullptr);
  void loadDocument(const QString &, bool replacementApproved);
  void registerStaticTexts();
  void retranslateUi();
  void showMonitorCapture(bool record = true);
  void buildUi();
  void openSettings();
  void applyPreferences();
  void updateSleepStatus();
  void buildGroups();
  void buildProfileGroups(const QString &selection);
  void buildModuleGroups(const QString &selection);
  void syncAccessColumn();
  QString columnLayoutKey() const;
  void restoreSavedColumns();
  void resetColumnOrder();
  void saveColumns();
  void loadColumns();
  void resetColumns();
  void queueColumnSave();
  void flushColumnSave();
  void resetGroup(bool);
  void renderValues(bool preserveContext=false);
  void renderProfileValues(bool preserveContext);
  void updateDocumentBanner();
  void editSelected();
  void commitPendingValueEditor();
  void selected();
  void showParameterHelp();
  void showAbout();
  void command(unsigned);
  bool finishChanges();
  void closeDocument();
  bool busy() const { return process_ || monitorFd_ >= 0; }
  void selectOpen();
  void selectSaveAs();
  void selectExport();
  void decodeRnd();
  QString selectInterface();
  void requestConfiguration();
  void requestControllerWrite();
  void requestIdentities();
  void startTool(const QString &, const QStringList &, bool config);
  void processFinished(int, QProcess::ExitStatus);
  void cancelTask();
  void receiveFrames();
  void stopMonitor(bool show = true);
  void hardwareInfo();
};
QString fromU16(const std::u16string &);
} // namespace rnet::qt
