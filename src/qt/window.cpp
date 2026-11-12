#include "window.hpp"
#include "i18n.hpp"
#include "value_editor.hpp"
#include "settings_dialog.hpp"
#include "can_heartbeat.hpp"
#include "sleep_inhibitor.hpp"
#include "hazard_blink_cycle.hpp"
#include "rnd_decoder.hpp"
#include "runtime_resources.hpp"
#include <QSocketNotifier>
#include <QScopedValueRollback>
#include <QtWidgets>
#include <cerrno>
#include <csignal>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <set>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace rnet::qt {
namespace {
QColor accessColor(int level){
  static const QColor colors[]{QColor("#FFEB3B"),QColor("#4CAF50"),QColor("#1976D2"),QColor("#D32F2F")};
  return level>=0&&level<4?colors[level]:QColor(Qt::gray);
}
class ChangedValueDelegate final : public QStyledItemDelegate {
public:
  std::function<ParameterValueEditor*(int,QWidget*)> factory;
  std::function<void(int,const QString&)> commit;
  using QStyledItemDelegate::QStyledItemDelegate;
  QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &,const QModelIndex &index) const override {
    if(!(index.flags()&Qt::ItemIsEditable) || !factory)return nullptr;
    auto *editor=factory(index.data(Qt::UserRole).toInt(),parent);
    if(!editor)return nullptr;
    editor->setProperty("valueIndex",index.data(Qt::UserRole));
    editor->installEventFilter(const_cast<ChangedValueDelegate*>(this));
    for(auto *child:editor->findChildren<QWidget*>())child->installEventFilter(const_cast<ChangedValueDelegate*>(this));
    return editor;
  }
  void setEditorData(QWidget *,const QModelIndex &) const override {}
  void setModelData(QWidget *widget,QAbstractItemModel *,const QModelIndex &) const override {
    auto *editor=dynamic_cast<ParameterValueEditor*>(widget);
    if(!editor || !editor->valid() || editor->property("committing").toBool())return;
    const auto input=editor->rawValue();editor->setProperty("committing",true);
    if(commit)commit(editor->property("valueIndex").toInt(),input);
  }
  bool eventFilter(QObject *object,QEvent *event) override {
    auto *parent=object;
    while(parent&&!dynamic_cast<ParameterValueEditor*>(parent))parent=parent->parent();
    auto *editor=dynamic_cast<ParameterValueEditor*>(parent);
    if(!editor)return QStyledItemDelegate::eventFilter(object,event);
    if(editor->property("committing").toBool())return false;
    if(event->type()==QEvent::KeyPress && !QApplication::activePopupWidget()){
      auto *key=static_cast<QKeyEvent*>(event);
      if(key->key()==Qt::Key_Escape){editor->setProperty("committing",true);emit closeEditor(editor,QAbstractItemDelegate::RevertModelCache);return true;}
      if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter||key->key()==Qt::Key_Tab||key->key()==Qt::Key_Backtab){
        QString error;
        if(!editor->valid(&error)){QToolTip::showText(editor->mapToGlobal(QPoint()),error,editor);return true;}
        QPointer<ParameterValueEditor> guard(editor);emit commitData(editor);
        if(guard)emit closeEditor(editor,QAbstractItemDelegate::NoHint);
        return true;
      }
    }
    if(event->type()==QEvent::FocusOut && !QApplication::activePopupWidget()){
      QPointer<ParameterValueEditor> guard(editor);
      QTimer::singleShot(0,this,[this,guard]{
        if(!guard||guard->property("committing").toBool()||QApplication::activePopupWidget())return;
        auto *focus=QApplication::focusWidget();if(focus&&(focus==guard||guard->isAncestorOf(focus)))return;
        QString error;
        if(guard->valid(&error))emit commitData(guard);
        else QToolTip::showText(guard->mapToGlobal(QPoint()),error,guard);
        if(guard)emit closeEditor(guard,QAbstractItemDelegate::NoHint);
      });
      return false;
    }
    return false;
  }
protected:
  void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option,index);
    if (index.data(Qt::FontRole).value<QFont>().bold() &&
        index.data(Qt::ForegroundRole).value<QColor>()==QColor(Qt::red))
      option->palette.setColor(QPalette::HighlightedText,Qt::red);
  }
};
constexpr unsigned Values = 40009, Repository = 40010, Edit = 40011,
                   Catalog = 40008, Resources = 40001, Export = 40004;
std::filesystem::path path(const QString &s) {
  return std::filesystem::path(s.toStdString());
}
QString hex(unsigned n) { return "0x" + QString::number(n, 16).toUpper(); }
QByteArray array(const Bytes &b) {
  return QByteArray(reinterpret_cast<const char *>(b.data()),
                    qsizetype(b.size()));
}
void writeNew(const QString &name, const Bytes &b) {
  QFile f(name);
  if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    throw UiError(
        (QCoreApplication::translate("RNet", "Cannot create a new file: ") +
         f.errorString())
            .toStdString());
  if (f.write(array(b)) != qsizetype(b.size()) || !f.flush()) {
    f.remove();
    throw UiError(QCoreApplication::translate(
        "RNet", "Could not save the complete file"));
  }
}
QString csv(QString s) {
  s.replace('"', "\"\"");
  return '"' + s + '"';
}
bool implemented(unsigned id) {
  return id == 57665 || id == 57664 || id == 57636 || id == 57601 ||
         id == 57603 || id == 57604 || id == 57643 || id == 32844 ||
         id == 32831 || id == 32939 || id == 32803 || id == 32804;
}
void fillMetadata(Row &row,const ParameterDefinition &parameter,bool numeric){
  auto prefix=fromU16(parameter.option_texts[0]);
  if(prefix==QStringLiteral("u"))prefix=QString::fromUtf8("µ");
  row.unit=prefix+fromU16(parameter.option_texts[2]);
  if(row.unit.isEmpty())row.unit=QStringLiteral("—");
  row.scale=parameter.metadata[8]&&parameter.metadata[9]?
      QStringLiteral("%1:%2").arg(parameter.metadata[8]).arg(parameter.metadata[9]):QStringLiteral("—");
  if(numeric){
    row.minimum=QString::number(parameter.signed_storage?
        qint64(qint32(parameter.metadata[4])):qint64(parameter.metadata[4]));
    row.maximum=QString::number(parameter.signed_storage?
        qint64(qint32(parameter.metadata[5])):qint64(parameter.metadata[5]));
  }else row.minimum=row.maximum=QStringLiteral("—");
  row.metadataSummary=QStringLiteral("%1 · %2 · %3–%4")
      .arg(row.unit,row.scale,row.minimum,row.maximum);
}
} // namespace
QString fromU16(const std::u16string &s) {
  return QString::fromStdU16String(s);
}
QVariant TableModel::data(const QModelIndex &i, int role) const {
  if (!i.isValid() || i.row() >= int(rows.size()))
    return {};
  const auto &r = rows[std::size_t(i.row())];
  if(profileLayout){
    const bool valueColumn=i.column()>=2&&i.column()<headings.size()-1;
    const auto *cell=valueColumn?&r.cells.at(std::size_t(i.column()-2)):nullptr;
    if(role==Qt::UserRole)return cell?cell->valueIndex:-1;
    if(role==Qt::DecorationRole&&i.column()==1&&r.icon!=Icon::None)return uiIcon(r.icon);
    if(r.unread&&i.column()!=headings.size()-1&&role==Qt::ForegroundRole)return QColor(Qt::gray);
    if(i.column()==headings.size()-1&&r.requiredAccess>=0&&r.requiredAccess<=3){
      if(role==Qt::BackgroundRole)return accessColor(r.requiredAccess);
      if(role==Qt::ForegroundRole)return QColor(r.requiredAccess>=2?Qt::white:Qt::black);
    }
    if(cell&&cell->changed){
      if(role==Qt::ForegroundRole)return QColor(Qt::red);
      if(role==Qt::FontRole){QFont font;font.setBold(true);return font;}
    }
    if(role==Qt::ToolTipRole)return cell&&!cell->detail.isEmpty()?cell->detail:r.detail;
    if(role!=Qt::DisplayRole)return {};
    if(i.column()==0)return r.id;
    if(i.column()==1)return r.name;
    if(cell)return cell->value;
    if(i.column()==headings.size()-1&&r.requiredAccess>=0)
      return QString::number(r.requiredAccess)+" · "+
          QCoreApplication::translate("RNet",access_level_name(AccessLevel(r.requiredAccess)));
    return {};
  }
  if (role == Qt::UserRole) return r.valueIndex;
  if (role == Qt::DecorationRole && i.column()==1 && r.icon!=Icon::None) return uiIcon(r.icon);
  if(r.unread&&i.column()!=headings.size()-1&&role==Qt::ForegroundRole)return QColor(Qt::gray);
  if (i.column() == headings.size()-1 && r.requiredAccess >= 0 && r.requiredAccess <= 3) {
    if (role == Qt::BackgroundRole)
      return accessColor(r.requiredAccess);
    if (role == Qt::ForegroundRole)
      return QColor(r.requiredAccess >= 2 ? Qt::white : Qt::black);
  }
  if (i.column() == 2 && r.changed) {
    if (role == Qt::ForegroundRole) return QColor(Qt::red);
    if (role == Qt::FontRole) {QFont font; font.setBold(true); return font;}
  }
  if (role == Qt::ToolTipRole){
    if((headings.size()==6||headings.size()==7)&&i.column()==3)
      return QCoreApplication::translate("RNet","Unit: %1\nScale: %2\nRaw min: %3\nRaw max: %4\nCatalog metadata; no unit conversion.")
          .arg(r.unit,r.scale,r.minimum,r.maximum);
    if(headings.size()==7&&i.column()==5)return r.module;
    return r.detail;
  }
  if (role != Qt::DisplayRole)
    return {};
  if(i.column()==headings.size()-1&&r.requiredAccess>=0)
    return QString::number(r.requiredAccess)+" · "+
        QCoreApplication::translate("RNet",access_level_name(AccessLevel(r.requiredAccess)));
  switch (i.column()) {
  case 0:
    return r.id;
  case 1:
    return r.name;
  case 2:
    return r.value;
  case 3:
    if(headings.size()==6||headings.size()==7)return r.metadataSummary;
    return r.location;
  case 4:
    if(headings.size()==6||headings.size()==7)return r.location;
    break;
  case 5:
    if(headings.size()==7)return r.module;
    break;
  }
  return {};
}
QVariant TableModel::headerData(int i, Qt::Orientation o, int role) const {
  if(role!=Qt::DisplayRole||o!=Qt::Horizontal||i<0||i>=headings.size())return {};
  QString title=headings[i];
  if(title==QCoreApplication::translate("RNet","Unit · Scale · Min–Max"))
    title.replace(QStringLiteral(" · Min"),QStringLiteral("\nMin"));
  else if(title==QCoreApplication::translate("RNet","Stored raw value")){
    const int breakAt=title.lastIndexOf(' ');
    if(breakAt>=0)title.replace(breakAt,1,QStringLiteral("\n"));
  }
  else if(title==QCoreApplication::translate("RNet","Profile / element")||
          title==QCoreApplication::translate("RNet","Module / group"))
    title.replace(QStringLiteral(" / "),QStringLiteral(" /\n"));
  return title;
}
void TableModel::replace(std::vector<Row> r, QStringList h, bool profiles) {
  beginResetModel();
  rows = std::move(r);
  headings = std::move(h);
  profileLayout=profiles;
  endResetModel();
}
void Filter::refilter() {
  beginFilterChange();
  endFilterChange();
}
bool Filter::filterAcceptsRow(int row, const QModelIndex &) const {
  auto *m = static_cast<TableModel *>(sourceModel());
  const auto &r = m->rows.at(std::size_t(row));
  return (r.requiredAccess < 0 || access_allows(unsigned(r.requiredAccess), access)) &&
         (group.isEmpty() || r.group == group ||
          r.group.startsWith(group + '~')) &&
         (query.isEmpty() || (r.id + ' ' + r.name + ' ' + r.value + ' ' + r.module +
             [&r] {QString values;for(const auto &cell:r.cells)values+=' '+cell.value;return values;}())
                                 .contains(query, Qt::CaseInsensitive));
}
Window::Window(const QString &output,
               QWidget *parent)
    : QMainWindow(parent),
      output_(output) {
  if (!QDir().mkpath(output_))
    throw UiError(
        QCoreApplication::translate("RNet", "Cannot create the log directory"));
  catalog_ = bundled_catalog();
  languages();
  settings_=AppSettings::load();
  access_=AccessLevel(settings_.accessLevel);
  buildUi();
  applyPreferences();
  showValues();
  columnLayoutReady_=true;
  updateTitle();
  log(QString("START Qt %1 Linux; parameters=%2; no Wine or DLL runtime")
          .arg(qVersion())
          .arg(catalog_.parameters.size()));
}
Window::~Window() { flushColumnSave();stopMonitor(false); }
void Window::log(const QString &text) {
  QFile f(output_ + "/runtime.log");
  if (f.open(QIODevice::WriteOnly | QIODevice::Append))
    f.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + ' ' +
             text + '\n')
                .toUtf8());
}
void Window::guarded(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::exception &e) {
    log("ERROR " + localizedError(e.what()));
    QMessageBox::warning(this, "dfreernet", localizedError(e.what()));
  }
}
QMenu *Window::makeMenu(const std::vector<MenuItem> &entries,
                                QMenu *parent) {
  QMenu *menu = parent ? parent : new QMenu(this);
  for (const auto &e : entries) {
    if (!e.children.empty()) {
      auto *sub = parent ? menu->addMenu(fromU16(e.text))
                         : menuBar()->addMenu(fromU16(e.text));
      makeMenu(e.children, sub);
    } else if (e.text.empty())
      menu->addSeparator();
    else {
      auto *a = menu->addAction(fromU16(e.text));
      a->setEnabled(implemented(e.id));
      a->setObjectName("command" + QString::number(e.id));
      a->setIcon(uiIcon(actionIcon(e.id)));
      actions_[e.id] = a;
      connect(a, &QAction::triggered, this,
              [this, id = e.id] { guarded([this, id] { command(id); }); });
    }
  }
  return menu;
}
void Window::buildUi() {
  setMinimumSize(820, 560);
  resize(1200, 820);
  setObjectName("rnetWindow");
  setWindowIcon(uiIcon(Icon::App));
  makeMenu(bundled_menus())->deleteLater();
  auto add = [this](QMenu *menu, const QString &text, unsigned id) {
    auto *a = menu->addAction(text);
    actions_[id] = a;
    a->setObjectName("command" + QString::number(id));
    a->setIcon(uiIcon(actionIcon(id)));
    connect(a, &QAction::triggered, this,
            [this, id] { guarded([this, id] { command(id); }); });
    return a;
  };
  auto *fileMenu=qobject_cast<QMenu*>(actions_.at(57601)->parent());
  if(!fileMenu)throw UiError("File menu is missing");
  auto *importAction=add(fileMenu,QT_TRANSLATE_NOOP("RNet","Import RND catalog…"),40002);
  if(actions_.count(57665))fileMenu->insertAction(actions_.at(57665),importAction);
  auto *editMenu=qobject_cast<QMenu*>(actions_.at(57636)->parent());
  if(!editMenu)throw UiError("Edit menu is missing");
  editMenu->addSeparator();auto *preferences=add(editMenu,QT_TRANSLATE_NOOP("RNet","&Settings…"),40040);
  preferences->setIcon(uiIcon(Icon::Settings));preferences->setShortcut(QKeySequence::Preferences);
  auto *dongle = menuBar()->addMenu("&Dongle");
  dongle->setObjectName("dongleMenu");
  auto *roles = new QActionGroup(this);
  roles->setExclusive(true);
  for (unsigned i = 0; i < 4; ++i) {
    auto *a = dongle->addAction(
        QString::fromLatin1(access_level_name(AccessLevel(i))));
    a->setCheckable(true);
    a->setChecked(i == unsigned(access_));
    a->setObjectName("role" + QString::number(i));
    a->setIcon(uiIcon(Icon::Key));
    roles->addAction(a);
    roleActions_.push_back(a);
    connect(a, &QAction::triggered, this,
            [this, i] {
              guarded([this,i]{
                if(i>=2&&i!=unsigned(access_)&&QMessageBox::warning(this,QCoreApplication::translate("RNet","Elevated Dongle level"),QCoreApplication::translate("RNet","OEM and Factory expose advanced parameters. Incorrect changes can affect wheelchair operation. Select this level for the current session?"),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
                setAccess(AccessLevel(i));
              });
              for(unsigned n=0;n<roleActions_.size();++n)roleActions_[n]->setChecked(n==unsigned(access_));
            });
  }
  dongle->addSeparator();
  auto *scope = dongle->addAction(QT_TRANSLATE_NOOP(
      "RNet", "Access level applies to local document editing"));
  scope->setEnabled(false);
  auto *controller = menuBar()->addMenu("&CAN (Linux)");
  add(controller, QT_TRANSLATE_NOOP("RNet", "Request and read configuration…"),
      40030);
  add(controller, QT_TRANSLATE_NOOP("RNet", "Read identifiers (C8/C9)…"),
      40031);
  add(controller, QT_TRANSLATE_NOOP("RNet", "Monitor CAN…"),
      40025);
  add(controller, QT_TRANSLATE_NOOP("RNet", "Show USB CAN devices"), 40003);
  controller->addSeparator();
  blinkAction_=controller->addAction(QT_TRANSLATE_NOOP("RNet","Live signal: hazard lights"));blinkAction_->setObjectName("hazardBlinkAction");blinkAction_->setCheckable(true);blinkAction_->setIcon(uiIcon(Icon::Light));
  connect(blinkAction_,&QAction::triggered,this,[this](bool checked){if(!checked){settings_.liveSignalEnabled=false;QSettings().setValue("can/liveSignalEnabled",false);blinkCycle_->stop();return;}blinkAction_->setChecked(false);guarded([this]{requestBlinkCycle();});});
  auto *view = menuBar()->addMenu(QT_TRANSLATE_NOOP("RNet", "&Views"));
  add(view, QT_TRANSLATE_NOOP("RNet", "Read parameter values"), Values);
  auto *layoutGroup=new QActionGroup(this);layoutGroup->setExclusive(true);
  listViewAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Hierarchical list"));
  profileViewAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Profile table"));
  moduleViewAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Module groups"));
  listViewAction_->setObjectName("hierarchicalListView");
  profileViewAction_->setObjectName("profileTableView");
  moduleViewAction_->setObjectName("moduleGroupView");
  for(auto *action:{listViewAction_,profileViewAction_,moduleViewAction_}){
    action->setCheckable(true);layoutGroup->addAction(action);
  }
  listViewAction_->setChecked(!settings_.profileTableView&&!settings_.moduleGroupView);
  profileViewAction_->setChecked(settings_.profileTableView);
  moduleViewAction_->setChecked(settings_.moduleGroupView);
  connect(listViewAction_,&QAction::triggered,this,[this]{
    guarded([this]{commitPendingValueEditor();settings_.profileTableView=false;settings_.moduleGroupView=false;
      QSettings q;q.setValue("ui/profileTableView",false);q.setValue("ui/moduleGroupView",false);renderValues();});
  });
  connect(profileViewAction_,&QAction::triggered,this,[this]{
    guarded([this]{commitPendingValueEditor();settings_.profileTableView=true;settings_.moduleGroupView=false;
      QSettings q;q.setValue("ui/profileTableView",true);q.setValue("ui/moduleGroupView",false);renderValues();});
  });
  connect(moduleViewAction_,&QAction::triggered,this,[this]{
    guarded([this]{commitPendingValueEditor();settings_.profileTableView=false;settings_.moduleGroupView=true;
      QSettings q;q.setValue("ui/profileTableView",false);q.setValue("ui/moduleGroupView",true);renderValues();});
  });
  view->addSeparator();
  saveColumnsAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Save column layout"));
  loadColumnsAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Load column layout"));
  resetColumnsAction_=view->addAction(QT_TRANSLATE_NOOP("RNet","Reset column layout"));
  saveColumnsAction_->setObjectName("saveColumnLayout");
  loadColumnsAction_->setObjectName("loadColumnLayout");
  resetColumnsAction_->setObjectName("resetColumnLayout");
  connect(saveColumnsAction_,&QAction::triggered,this,[this]{guarded([this]{saveColumns();});});
  connect(loadColumnsAction_,&QAction::triggered,this,[this]{guarded([this]{loadColumns();});});
  connect(resetColumnsAction_,&QAction::triggered,this,[this]{guarded([this]{resetColumns();});});
  add(view, "Repository-Dateien", Repository);
  add(view, QT_TRANSLATE_NOOP("RNet", "Bundled resource texts"), Resources);
  view->addSeparator();
  auto *detail = view->addAction(QT_TRANSLATE_NOOP("RNet", "Show details"));
  detail->setIcon(uiIcon(Icon::Info));
  detailsAction_=detail;
  detail->setCheckable(true);
  detail->setChecked(true);
  auto *tools = menuBar()->addMenu(QT_TRANSLATE_NOOP("RNet", "&Analysis"));
  add(tools, QT_TRANSLATE_NOOP("RNet", "Edit selected raw value…"), Edit);
  add(tools, QT_TRANSLATE_NOOP("RNet", "Export view as CSV…"), Export);
  const std::map<unsigned, QKeySequence> shortcuts{
      {57601, QKeySequence::Open},   {57603, QKeySequence::Save},
      {57604, QKeySequence::SaveAs}, {57643, QKeySequence::Undo},
      {57636, QKeySequence::Find},   {57665, QKeySequence::Quit},
      {32939, QKeySequence::Close}};
  for (const auto &entry : shortcuts)
    if (actions_.count(entry.first))
      actions_[entry.first]->setShortcut(entry.second);
  auto *toolbar = addToolBar(QT_TRANSLATE_NOOP("RNet", "Controller and files"));
  toolbar->setObjectName("readFileToolbar");
  toolbar->setMovable(false);
  actions_.at(57664)->setText(QT_TRANSLATE_NOOP("RNet", "&About dfreernet"));
  auto *helpMenu=qobject_cast<QMenu*>(actions_.at(57664)->parent());
  if(!helpMenu)throw UiError("Help menu is missing");
  parameterHelpAction_=new QAction(QT_TRANSLATE_NOOP("RNet","Parameter help"),this);
  parameterHelpAction_->setObjectName("parameterHelp");
  parameterHelpAction_->setIcon(uiIcon(Icon::Info));
  parameterHelpAction_->setShortcut(QKeySequence(Qt::Key_F1));
  helpMenu->insertAction(actions_.at(57664),parameterHelpAction_);
  connect(parameterHelpAction_,&QAction::triggered,this,
          [this]{guarded([this]{showParameterHelp();});});
  actions_.at(40030)->setText(QT_TRANSLATE_NOOP("RNet", "Read controller…"));
  actions_.at(57601)->setText(QT_TRANSLATE_NOOP("RNet", "Read file…"));
  actions_.at(57604)->setText(QT_TRANSLATE_NOOP("RNet", "Write file as…"));
  actions_.at(57603)->setText(QT_TRANSLATE_NOOP("RNet", "Save file"));
  toolbar->addAction(actions_.at(40030));
  toolbar->addAction(actions_.at(32804));
  toolbar->addSeparator();
  toolbar->addAction(actions_.at(57601));
  toolbar->addAction(actions_.at(57604));
  toolbar->addAction(actions_.at(57603));
  auto *languageMenu =
      menuBar()->addMenu(QT_TRANSLATE_NOOP("RNet", "Language"));
  languageMenu->setObjectName("languageMenu");
  auto *languageGroup = new QActionGroup(this);
  languageGroup->setExclusive(true);
  for (const auto &entry : std::vector<std::pair<QString, QString>>{
           {"en_US", QT_TRANSLATE_NOOP("RNet", "English (US)")},
           {"de_DE", QT_TRANSLATE_NOOP("RNet", "Deutsch")}}) {
    auto *action = languageMenu->addAction(entry.second);
    action->setCheckable(true);
    action->setObjectName("language_" + entry.first);
    action->setData(entry.first);
    languageGroup->addAction(action);
    languageActions_.push_back(action);
    connect(action, &QAction::triggered, this, [this, code = entry.first] {
      guarded([this,code] { commitPendingValueEditor(); languages().setLanguage(code, true); });
    });
  }
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(12, 10, 12, 8);
  banner_ = new QLabel;
  banner_->setWordWrap(true);
  banner_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  banner_->setObjectName("provenanceBanner");
  layout->addWidget(banner_);
  auto *searchBar = new QHBoxLayout;
  search_ = new QLineEdit;
  search_->setObjectName("search");
  search_->setPlaceholderText(
      QT_TRANSLATE_NOOP("RNet", "Search parameters by name, ID, or raw value"));
  search_->setClearButtonEnabled(true);
  searchBar->addWidget(search_, 1);
  auto *listViewButton=new QToolButton;
  listViewButton->setObjectName("hierarchicalListButton");
  listViewButton->setDefaultAction(listViewAction_);
  listViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  listViewButton->setAutoRaise(false);
  searchBar->addWidget(listViewButton);
  auto *profileViewButton=new QToolButton;
  profileViewButton->setObjectName("profileTableButton");
  profileViewButton->setDefaultAction(profileViewAction_);
  profileViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  profileViewButton->setAutoRaise(false);
  searchBar->addWidget(profileViewButton);
  auto *moduleViewButton=new QToolButton;
  moduleViewButton->setObjectName("moduleGroupButton");
  moduleViewButton->setDefaultAction(moduleViewAction_);
  moduleViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  moduleViewButton->setAutoRaise(false);
  searchBar->addWidget(moduleViewButton);
  edit_ = new QPushButton(QT_TRANSLATE_NOOP("RNet", "Edit value…"));
  edit_->setObjectName("editValue");
  edit_->setIcon(uiIcon(Icon::Edit));
  undoButton_ = new QPushButton(QT_TRANSLATE_NOOP("RNet", "Undo"));
  undoButton_->setIcon(uiIcon(Icon::Undo));
  searchBar->addWidget(edit_);
  searchBar->addWidget(undoButton_);
  layout->addLayout(searchBar);
  auto *vertical = new QSplitter(Qt::Vertical);
  auto *horizontal = new QSplitter(Qt::Horizontal);
  tree_ = new QTreeWidget;
  tree_->setObjectName("groups");
  tree_->installEventFilter(this);
  tree_->setHeaderHidden(true);
  tree_->setIconSize(QSize(18,18));
  tree_->setMinimumWidth(180);
  horizontal->addWidget(tree_);
  table_ = new QTableView;
  table_->setObjectName("parameters");
  table_->installEventFilter(this);
  auto *delegate=new ChangedValueDelegate(table_);
  auto editable=[this](int i){
    if(busy()||!valuesView_||!document_||i<0||i>=int(values_.size()))return false;
    const auto &v=values_[i];const auto &p=catalog_.parameters.at(v.parameter);
    return access_allows(p.metadata[1]>>5,access_)&&supports_direct_edit(p,v,&catalog_);
  };
  delegate->factory=[this,editable](int i,QWidget *parent)->ParameterValueEditor*{
    if(!editable(i))return nullptr;
    const auto &v=values_[i];return new ParameterValueEditor(catalog_.parameters.at(v.parameter),v,catalog_,parent);
  };
  delegate->commit=[this](int i,const QString &value){guarded([this,i,value]{applyEdit(i,value);});};
  table_->setItemDelegate(delegate);
  table_->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
  model_ = new TableModel(this);
  model_->canEdit=editable;
  proxy_ = new Filter(this);
  proxy_->access=access_;
  proxy_->setSourceModel(model_);
  table_->setModel(proxy_);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
  table_->setAlternatingRowColors(true);
  table_->setIconSize(QSize(18,18));
  table_->setSortingEnabled(true);
  table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  table_->verticalHeader()->hide();
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  table_->horizontalHeader()->setSectionsMovable(true);
  table_->horizontalHeader()->setStretchLastSection(false);
  columnSaveTimer_=new QTimer(this);
  columnSaveTimer_->setSingleShot(true);
  columnSaveTimer_->setInterval(250);
  connect(columnSaveTimer_,&QTimer::timeout,this,[this]{flushColumnSave();});
  connect(table_->horizontalHeader(),&QHeaderView::sectionResized,this,
          [this](int, int, int){queueColumnSave();});
  connect(table_->horizontalHeader(),&QHeaderView::sectionMoved,this,
          [this](int, int, int){queueColumnSave();});
  auto *tableContainer=new QWidget;
  auto *tableLayout=new QHBoxLayout(tableContainer);
  tableLayout->setContentsMargins(0,0,0,0);
  tableLayout->setSpacing(0);
  tableLayout->addWidget(table_,1);
  accessTable_=new QTableView;
  accessTable_->setObjectName("fixedAccessColumn");
  accessTable_->setModel(proxy_);
  accessTable_->setFixedWidth(165);
  accessTable_->setSelectionMode(QAbstractItemView::NoSelection);
  accessTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  accessTable_->setFocusPolicy(Qt::NoFocus);
  accessTable_->setAlternatingRowColors(true);
  accessTable_->verticalHeader()->hide();
  accessTable_->horizontalHeader()->setStretchLastSection(true);
  accessTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  accessTable_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tableLayout->addWidget(accessTable_);
  connect(table_->verticalScrollBar(),&QScrollBar::valueChanged,
          accessTable_->verticalScrollBar(),&QScrollBar::setValue);
  connect(accessTable_->verticalScrollBar(),&QScrollBar::valueChanged,
          table_->verticalScrollBar(),&QScrollBar::setValue);
  horizontal->addWidget(tableContainer);
  horizontal->setStretchFactor(1, 1);
  horizontal->setSizes({310, 950});
  vertical->addWidget(horizontal);
  details_ = new QPlainTextEdit;
  details_->setObjectName("details");
  details_->setReadOnly(true);
  vertical->addWidget(details_);
  vertical->setStretchFactor(0, 1);
  vertical->setSizes({600, 140});
  layout->addWidget(vertical, 1);
  taskPanel_ = new QWidget;
  auto *taskLayout = new QVBoxLayout(taskPanel_);
  taskLayout->setContentsMargins(0, 0, 0, 0);
  taskLog_ = new QPlainTextEdit;
  taskLog_->setReadOnly(true);
  taskLog_->setMaximumHeight(170);
  taskLog_->document()->setMaximumBlockCount(4000);
  cancel_ =
      new QPushButton(QT_TRANSLATE_NOOP("RNet", "Cancel / release access"));
  taskLayout->addWidget(taskLog_);
  cancel_->setIcon(uiIcon(Icon::Stop));
  taskLayout->addWidget(cancel_);
  layout->addWidget(taskPanel_);
  taskPanel_->hide();
  setCentralWidget(central);
  connect(search_, &QLineEdit::textChanged, this, [this](const QString &s) {
    proxy_->query = s;
    proxy_->refilter();
    updateActions();
  });
  connect(tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *item) {
            proxy_->group =
                item ? item->data(0, Qt::UserRole).toString() : QString();
            proxy_->refilter();
            updateActions();
          });
  connect(table_->selectionModel(), &QItemSelectionModel::currentChanged,
          this, [this] { selected(); });
  connect(edit_, &QPushButton::clicked, this,
          [this] { guarded([this] { editSelected(); }); });
  connect(undoButton_, &QPushButton::clicked, this,
          [this] { guarded([this] { undo(); }); });
  connect(detail,&QAction::toggled,this,[this](bool visible){details_->setVisible(visible);settings_.showDetails=visible;QSettings().setValue("ui/showDetails",visible);});
  accessLabel_=new QLabel;accessLabel_->setObjectName("dongleStatus");statusBar()->addPermanentWidget(accessLabel_);
  blinkLabel_ = new QLabel;
  blinkLabel_->setObjectName("lightStatus");
  blinkLabel_->setFixedSize(28, 26);
  blinkLabel_->setAlignment(Qt::AlignCenter);
  statusBar()->addPermanentWidget(blinkLabel_);
  blinkLabel_->hide();
  blinkCycle_=new HazardBlinkCycle(this);blinkCycle_->canPulse=[this]{return !busy()&&heartbeat_&&heartbeat_->receiving();};blinkCycle_->onChanged=[this]{updateBlinkState();};blinkCycle_->onEvent=[this](const QString &message){log(message);};
  sleepLabel_=new QLabel;statusBar()->addPermanentWidget(sleepLabel_);
  sleepInhibitor_=new SleepInhibitor(this);sleepInhibitor_->onChanged=[this]{updateSleepStatus();};
  heartbeat_=new CanHeartbeatIndicator(this);statusBar()->addPermanentWidget(heartbeat_);
  connect(cancel_, &QPushButton::clicked, this, [this] { cancelTask(); });
  registerStaticTexts();
  uiReady_ = true;
  for (const auto &binding : textBindings_)
    binding.object->setProperty(
        binding.property.constData(),
        QCoreApplication::translate("RNet", binding.source.constData()));
  for (auto *action : languageActions_)
    action->setChecked(action->data().toString() == languages().language());
}
void Window::registerStaticTexts() {
  auto add = [this](QObject *object, const char *property) {
    auto source = object->property(property).toString();
    if (!source.isEmpty())
      textBindings_.push_back({object, property, source.toUtf8()});
  };
  for (auto *action : findChildren<QAction *>())
    add(action, "text");
  for (auto *button : findChildren<QPushButton *>())
    add(button, "text");
  add(search_, "placeholderText");
  for (auto *toolbar : findChildren<QToolBar *>())
    add(toolbar, "windowTitle");
}
bool Window::eventFilter(QObject *object,QEvent *event) {
  if((object==table_||object==tree_)&&event->type()==QEvent::KeyPress&&
      static_cast<QKeyEvent*>(event)->key()==Qt::Key_F1&&
      parameterHelpAction_&&parameterHelpAction_->isEnabled()){
    parameterHelpAction_->trigger();
    return true;
  }
  return QMainWindow::eventFilter(object,event);
}
void Window::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::LanguageChange && uiReady_)
    retranslateUi();
}
void Window::retranslateUi() {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  updateSleepStatus();updateBlinkState();
  const auto query = search_->text(), group = proxy_->group;
  QSet<QString> expanded;
  for (QTreeWidgetItemIterator item(tree_); *item; ++item)
    if ((*item)->isExpanded())
      expanded.insert((*item)->data(0, Qt::UserRole).toString());
  const auto current = proxy_->mapToSource(table_->currentIndex());
  int row = current.isValid() ? current.row() : -1;
  int valueIndex = current.isValid() ? current.data(Qt::UserRole).toInt() : -1;
  int column = table_->horizontalHeader()->sortIndicatorSection();
  auto order = table_->horizontalHeader()->sortIndicatorOrder();
  std::vector<int> widths(std::size_t(model_->columnCount()));
  for (int i = 0; i < model_->columnCount(); ++i)
    widths[i] = table_->columnWidth(i);
  int scroll = table_->verticalScrollBar()->value();
  for (const auto &binding : textBindings_)
    if (binding.object)
      binding.object->setProperty(
          binding.property.constData(),
          QCoreApplication::translate("RNet", binding.source.constData()));
  switch (view_) {
  case View::Values:
    renderValues();
    break;
  case View::Catalog:
    showCatalog();
    break;
  case View::Repository:
    showRepository();
    break;
  case View::Resources:
    showResources();
    break;
  case View::Monitor:
    showMonitorCapture(false);
    break;
  }
  search_->setText(query);
  for (QTreeWidgetItemIterator item(tree_); *item; ++item) {
    auto path = (*item)->data(0, Qt::UserRole).toString();
    (*item)->setExpanded(expanded.contains(path));
    if (path == group)
      tree_->setCurrentItem(*item);
  }
  table_->sortByColumn(column, order);
  for (int i = 0; i < model_->columnCount()&&i<int(widths.size()); ++i)
    table_->setColumnWidth(i, widths[i]);
  bool restored=false;
  if(valueIndex>=0)
    for(int candidate=0;candidate<model_->rowCount()&&!restored;++candidate)
      for(int cell=0;cell<model_->columnCount()-1;++cell)
        if(model_->index(candidate,cell).data(Qt::UserRole).toInt()==valueIndex){
          table_->setCurrentIndex(proxy_->mapFromSource(model_->index(candidate,cell)));
          restored=true;break;
        }
  if(!restored&&row>=0&&row<model_->rowCount())
    table_->setCurrentIndex(proxy_->mapFromSource(model_->index(row,0)));
  table_->verticalScrollBar()->setValue(scroll);
  if (monitorFd_ >= 0)
    cancel_->setText(QCoreApplication::translate("RNet", "Stop capture"));
  else
    cancel_->setText(QCoreApplication::translate(
        "RNet", configTask_ ? "Cancel / release access" : "Cancel"));
  for (auto *action : languageActions_)
    action->setChecked(action->data().toString() == languages().language());
  updateActions();
  updateTitle();
}
void Window::buildGroups() {
  const QSignalBlocker blocked(tree_);
  tree_->clear();
  auto *root = new QTreeWidgetItem(
      tree_,
      {valuesView_ ? QCoreApplication::translate("RNet", settings_.showAllParameters?"Parameters":"Read parameters")
                   : QCoreApplication::translate("RNet", "Parameter catalog")});
  root->setData(0, Qt::UserRole, QString());
  root->setIcon(0,uiIcon(valuesView_?Icon::Settings:Icon::Catalog));
  std::map<QString, QTreeWidgetItem *> groups{{"", root}};
  for (const auto &row : model_->rows) {
    if (row.requiredAccess >= 0 && !access_allows(unsigned(row.requiredAccess), access_))
      continue;
    QString group = row.group, full;
    auto *parent = root;
    for (const auto &piece : group.split('~', Qt::SkipEmptyParts)) {
      if (!full.isEmpty())
        full += '~';
      full += piece;
      auto it = groups.find(full);
      if (it != groups.end())
        parent = it->second;
      else {
        auto *child = new QTreeWidgetItem(parent, {piece});
        child->setData(0, Qt::UserRole, full);
        child->setIcon(0,uiIcon(groupIcon(full)));
        groups.emplace(full, child);
        parent = child;
      }
    }
  }
  tree_->sortItems(0, Qt::AscendingOrder);
  root->setExpanded(true);
  tree_->setCurrentItem(root);
}
void Window::buildProfileGroups(const QString &selection) {
  const QSignalBlocker blocked(tree_);
  tree_->clear();
  QTreeWidgetItem *selected=nullptr;
  for(int scope=0;scope<=8;++scope){
    const QString key=scope?QStringLiteral("profile%1").arg(scope):QStringLiteral("common");
    const QString label=scope?QString(QCoreApplication::translate("RNet","Profile %1")).arg(scope):
        QCoreApplication::translate("RNet","Common");
    auto *item=new QTreeWidgetItem(tree_,{label});
    item->setData(0,Qt::UserRole,key);
    item->setIcon(0,uiIcon(scope?Icon::Settings:Icon::Catalog));
    if(scope>0){
      bool loaded=false;
      for(const auto &value:values_)if(value.group&&int(value.profile)+1==scope){
        loaded=true;break;
      }
      if(!loaded)item->setForeground(0,QBrush(Qt::gray));
    }
    if(key==(selection.isEmpty()?QStringLiteral("common"):selection))selected=item;
  }
  tree_->setVisible(true);
  if(!selected)selected=tree_->topLevelItem(0);
  tree_->setCurrentItem(selected);
  proxy_->group=selected->data(0,Qt::UserRole).toString();
  proxy_->refilter();
}
void Window::buildModuleGroups(const QString &selection){
  const QSignalBlocker blocked(tree_);
  tree_->clear();
  auto *root=new QTreeWidgetItem(tree_,{QCoreApplication::translate("RNet","All modules")});
  root->setData(0,Qt::UserRole,QString());
  root->setIcon(0,uiIcon(Icon::Catalog));
  QSet<QString> seen;
  QTreeWidgetItem *selected=root;
  for(const auto &row:model_->rows){
    if(row.requiredAccess>=0&&!access_allows(unsigned(row.requiredAccess),access_))continue;
    if(row.group.isEmpty()||seen.contains(row.group))continue;
    seen.insert(row.group);
    QString label=row.group;label.replace('~',QStringLiteral(" / "));
    auto *item=new QTreeWidgetItem(root,{label});
    item->setData(0,Qt::UserRole,row.group);
    item->setIcon(0,uiIcon(Icon::Settings));
    if(row.group==selection)selected=item;
  }
  root->sortChildren(0,Qt::AscendingOrder);
  root->setExpanded(true);
  tree_->setVisible(true);
  tree_->setCurrentItem(selected);
  proxy_->group=selected->data(0,Qt::UserRole).toString();
  proxy_->refilter();
}
void Window::resetGroup(bool show) {
  if (show)
    buildGroups();
  tree_->setVisible(show);
  proxy_->group.clear();
  search_->clear();
  tree_->setCurrentItem(tree_->topLevelItem(0));
  proxy_->refilter();
}
void Window::showCatalog() {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  view_ = View::Catalog;
  valuesView_ = false;
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  std::vector<Row> rows;
  for (const auto &entry : catalog_.parameters) {
    const auto &p = entry.second;
    rows.push_back({QString::number(p.key),
                    fromU16(p.name()),
                    {},
                    {},
                    fromU16(p.text88),
                    fromU16(p.help()),
                    -1, int(p.metadata[1] >> 5)});
    rows.back().icon=parameterIcon(p,catalog_);
  }
  model_->replace(std::move(rows),
                  {QCoreApplication::translate("RNet", "Parameter ID"),
                   QCoreApplication::translate("RNet", "Parameter (catalog)"),
                   "", "", QCoreApplication::translate("RNet", "Access level")});
  resetGroup(true);
  table_->setColumnHidden(2, true);
  table_->setColumnHidden(3, true);
  table_->setColumnWidth(4, 180);
  table_->setColumnWidth(0, 130);
  table_->setColumnWidth(1, 700);
  table_->sortByColumn(1, Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  banner_->setText(QCoreApplication::translate(
      "RNet", "Parameter definitions from the original RND. The catalog does "
              "not contain values read from the controller."));
  details_->setPlainText(QCoreApplication::translate(
      "RNet", "Select a parameter to read its original help text."));
  updateActions();
}
void Window::renderValues(bool preserveContext) {
  if(settings_.profileTableView){renderProfileValues(preserveContext);return;}
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  const auto query=search_->text(),group=proxy_->group;
  const auto current=proxy_->mapToSource(table_->currentIndex());
  const int selectedValue=current.isValid()?current.data(Qt::UserRole).toInt():-1;
  const int selectedColumn=current.isValid()&&!model_->profileLayout?current.column():2;
  const int sortColumn=table_->horizontalHeader()->sortIndicatorSection();
  const auto sortOrder=table_->horizontalHeader()->sortIndicatorOrder();
  const int scroll=table_->verticalScrollBar()->value();
  QSet<QString> expanded;
  if(preserveContext)for(QTreeWidgetItemIterator item(tree_);*item;++item)
    if((*item)->isExpanded())expanded.insert((*item)->data(0,Qt::UserRole).toString());
  view_ = View::Values;
  valuesView_ = true;
  if (!document_&&!settings_.showAllParameters) {
    model_->replace({}, {QCoreApplication::translate("RNet", "Parameter ID"), "Parameter",
             QCoreApplication::translate("RNet", "Read raw value"),
             QCoreApplication::translate("RNet", "Unit · Scale · Min–Max"),
             QCoreApplication::translate("RNet", "Profile / element"),
             QCoreApplication::translate("RNet", "Access level")});
    if(settings_.moduleGroupView)buildModuleGroups({});else resetGroup(true);
    table_->setColumnHidden(2, false);
    table_->setColumnHidden(3, false);
    table_->setColumnWidth(0,130);table_->setColumnWidth(1,620);
    table_->setColumnWidth(2,200);table_->setColumnWidth(3,240);
    table_->setColumnWidth(4,180);
    resetColumnOrder();
    restoreSavedColumns();
    banner_->setText(QCoreApplication::translate(
        "RNet", "No configuration has been read. Read the controller or open "
                "an R-net file."));
    details_->clear();
    updateActions();
    return;
  }
  std::vector<Row> rows;
  std::vector<std::string> baselineWarnings;
  auto baselineValues = document_?read_values(RnetDocument::read(controllerBaseline_), catalog_, &baselineWarnings):std::vector<DocumentValue>{};
  for (std::size_t i = 0; i < values_.size(); ++i) {
    const auto &v = values_[i];
    const auto &p = catalog_.parameters.at(v.parameter);
    const auto &f = document_->files[v.file];
    QString where =
        (v.group ? QString(QCoreApplication::translate("RNet", "Profile %1"))
                       .arg(v.profile + 1)
                 : QString(QCoreApplication::translate("RNet", "Common"))) +
        QString(" / %1").arg(v.element + 1);
    QString detail =
        fromU16(p.help()) +
        QString(
            QCoreApplication::translate(
                "RNet",
                "\n\nStored raw value; no unit conversion.\nFile %1, version "
                "%2\nSection %3, byte offset %4, width %5\nAccess level: %6"))
            .arg(hex(f.id))
            .arg(f.version)
            .arg(v.section + 1)
            .arg(v.offset)
            .arg(v.width)
            .arg(QCoreApplication::translate(
                "RNet", access_level_name(AccessLevel(p.metadata[1] >> 5))));
    rows.push_back({QString::number(v.parameter), fromU16(p.name()),
                    v.numeric ? QString::number(v.number) : fromU16(v.text),
                    where, fromU16(p.text88), detail, int(i), int(p.metadata[1] >> 5),
                    i >= baselineValues.size() || v.number != baselineValues[i].number ||
                    v.text != baselineValues[i].text});
    rows.back().icon=parameterIcon(p,catalog_);
    fillMetadata(rows.back(),p,v.numeric);
    if(settings_.moduleGroupView){
      rows.back().module=rows.back().group;
      rows.back().module.replace('~',QStringLiteral(" / "));
      rows.back().group=rows.back().group.section('~',0,1);
    }
  }
  if(settings_.showAllParameters){
    QSet<unsigned> present;for(const auto &value:values_)present.insert(value.parameter);
    for(const auto &[id,p]:catalog_.parameters){
      if(present.contains(id))continue;
      Row row{QString::number(id),fromU16(p.name()),QCoreApplication::translate("RNet","Not read"),QStringLiteral("—"),fromU16(p.text88),fromU16(p.help()),-1,int(p.metadata[1]>>5)};
      row.icon=parameterIcon(p,catalog_);row.unread=true;
      fillMetadata(row,p,p.metadata[4]||p.metadata[5]||p.metadata[9]);
      if(settings_.moduleGroupView){row.module=row.group;row.module.replace('~',QStringLiteral(" / "));row.group=row.group.section('~',0,1);}
      rows.push_back(std::move(row));
    }
  }
  model_->replace(std::move(rows),
                  {QCoreApplication::translate("RNet", "Parameter ID"),
                   "Parameter",
                   QCoreApplication::translate("RNet", "Stored raw value"),
                   QCoreApplication::translate("RNet", "Unit · Scale · Min–Max"),
                   QCoreApplication::translate("RNet", "Profile / element"),
             QCoreApplication::translate("RNet", "Access level")});
  if(settings_.moduleGroupView)buildModuleGroups(preserveContext?group:QString());
  else resetGroup(true);
  table_->setColumnHidden(2, false);
  table_->setColumnHidden(3, false);
  table_->setColumnWidth(3,240);
  table_->setColumnWidth(4,180);
  table_->setColumnWidth(0, 130);
  table_->setColumnWidth(1, 620);
  table_->setColumnWidth(2, 200);
  table_->sortByColumn(1, Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  if(document_)updateDocumentBanner();else banner_->setText(QCoreApplication::translate("RNet","No configuration has been read. Read the controller or open an R-net file."));
  QString details = documentPath_;
  if (!warnings_.empty())
    details +=
        QString(QCoreApplication::translate(
                    "RNet", "\n\n%1 file version(s) preserved as complete raw "
                            "data, not yet interpreted as individual values."))
            .arg(warnings_.size());
  details_->setPlainText(details);
  if(preserveContext){
    search_->setText(query);
    for(QTreeWidgetItemIterator item(tree_);*item;++item){
      auto path=(*item)->data(0,Qt::UserRole).toString();
      (*item)->setExpanded(expanded.contains(path));
      if(path==group)tree_->setCurrentItem(*item);
    }
    table_->sortByColumn(sortColumn,sortOrder);
    for(int row=0;row<model_->rowCount();++row)if(model_->rows[row].valueIndex==selectedValue){
      table_->setCurrentIndex(proxy_->mapFromSource(model_->index(row,selectedColumn)));break;
    }
    table_->verticalScrollBar()->setValue(scroll);
  }
  updateActions();
}
void Window::renderProfileValues(bool preserveContext) {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  const auto query=search_->text(),group=proxy_->group;
  const auto current=proxy_->mapToSource(table_->currentIndex());
  const int selectedValue=current.isValid()?current.data(Qt::UserRole).toInt():-1;
  const int sortColumn=table_->horizontalHeader()->sortIndicatorSection();
  const auto sortOrder=table_->horizontalHeader()->sortIndicatorOrder();
  const int scroll=table_->verticalScrollBar()->value();
  view_=View::Values;valuesView_=true;
  int profileCount=0;
  for(const auto &value:values_)if(value.group)profileCount=std::max(profileCount,int(value.profile)+1);
  std::vector<Row> rows;
  std::map<std::tuple<unsigned,unsigned,int>,int> occurrences;
  std::map<std::tuple<unsigned,unsigned,int>,std::size_t> rowNumbers;
  std::vector<std::string> baselineWarnings;
  auto baselineValues=document_?
      read_values(RnetDocument::read(controllerBaseline_),catalog_,&baselineWarnings):
      std::vector<DocumentValue>{};
  for(std::size_t i=0;i<values_.size();++i){
    const auto &v=values_[i];
    const auto &p=catalog_.parameters.at(v.parameter);
    const auto &f=document_->files[v.file];
    const int cellNumber=v.group?int(v.profile)+1:0;
    const auto occurrence=occurrences[{v.parameter,v.element,cellNumber}]++;
    const auto key=std::make_tuple(v.parameter,v.element,occurrence);
    auto found=rowNumbers.find(key);
    if(found==rowNumbers.end()){
      Row row;
      row.id=QString::number(v.parameter);
      row.name=fromU16(p.name());
      if(v.element)row.name+=QStringLiteral(" / %1").arg(v.element+1);
      row.group=fromU16(p.text88);
      row.detail=fromU16(p.help());
      row.requiredAccess=int(p.metadata[1]>>5);
      row.icon=parameterIcon(p,catalog_);
      row.cells.resize(std::size_t(profileCount+1));
      found=rowNumbers.emplace(key,rows.size()).first;
      rows.push_back(std::move(row));
    }
    auto &cell=rows.at(found->second).cells.at(std::size_t(cellNumber));
    cell.value=v.numeric?QString::number(v.number):fromU16(v.text);
    cell.valueIndex=int(i);
    cell.changed=i>=baselineValues.size()||v.number!=baselineValues[i].number||
                 v.text!=baselineValues[i].text;
    cell.detail=fromU16(p.help())+
        QString(QCoreApplication::translate("RNet",
          "\n\nStored raw value; no unit conversion.\nFile %1, version %2\nSection %3, byte offset %4, width %5\nAccess level: %6"))
          .arg(hex(f.id)).arg(f.version).arg(v.section+1).arg(v.offset)
          .arg(v.width).arg(QCoreApplication::translate(
              "RNet",access_level_name(AccessLevel(p.metadata[1]>>5))));
  }
  if(settings_.showAllParameters){
    QSet<unsigned> present;for(const auto &value:values_)present.insert(value.parameter);
    for(const auto &[id,p]:catalog_.parameters){
      if(present.contains(id))continue;
      Row row;
      row.id=QString::number(id);row.name=fromU16(p.name());
      row.group=fromU16(p.text88);row.detail=fromU16(p.help());
      row.requiredAccess=int(p.metadata[1]>>5);
      row.icon=parameterIcon(p,catalog_);row.unread=true;
      row.cells.resize(std::size_t(profileCount+1));
      row.cells.front().value=QCoreApplication::translate("RNet","Not read");
      rows.push_back(std::move(row));
    }
  }
  std::vector<Row> scoped;
  scoped.reserve(rows.size()*9);
  for(int scope=0;scope<=8;++scope){
    const QString key=scope?QStringLiteral("profile%1").arg(scope):QStringLiteral("common");
    for(const auto &source:rows){
      const Row::Cell *cell=scope<int(source.cells.size())?
          &source.cells[std::size_t(scope)]:nullptr;
      if((!cell||cell->valueIndex<0)&&!settings_.showAllParameters)continue;
      Row row;
      row.id=source.id;row.name=source.name;
      row.value=cell&&cell->valueIndex>=0?cell->value:
          QCoreApplication::translate("RNet","Not read");
      row.location=cell&&cell->valueIndex>=0?
          QString::number(values_.at(std::size_t(cell->valueIndex)).element+1):QStringLiteral("—");
      row.group=key;
      row.module=source.group;
      row.module.replace('~',QStringLiteral(" / "));
      const auto &parameter=catalog_.parameters.at(row.id.toUInt());
      const bool numeric=cell&&cell->valueIndex>=0?
          values_.at(std::size_t(cell->valueIndex)).numeric:
          (parameter.metadata[4]||parameter.metadata[5]||parameter.metadata[9]);
      fillMetadata(row,parameter,numeric);
      row.detail=cell&&!cell->detail.isEmpty()?cell->detail:source.detail;
      row.valueIndex=cell?cell->valueIndex:-1;
      row.requiredAccess=source.requiredAccess;
      row.changed=cell&&cell->changed;
      row.icon=source.icon;
      row.unread=row.valueIndex<0;
      scoped.push_back(std::move(row));
    }
  }
  model_->replace(std::move(scoped),
      {QCoreApplication::translate("RNet","Parameter ID"),
       QCoreApplication::translate("RNet","Parameter"),
       QCoreApplication::translate("RNet","Stored raw value"),
       QCoreApplication::translate("RNet","Unit · Scale · Min–Max"),
       QCoreApplication::translate("RNet","Element"),
       QCoreApplication::translate("RNet","Module / group"),
       QCoreApplication::translate("RNet","Access level")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  search_->clear();
  buildProfileGroups(preserveContext?group:QStringLiteral("common"));
  for(int col=0;col<model_->columnCount();++col)table_->setColumnHidden(col,false);
  table_->setColumnWidth(0,130);table_->setColumnWidth(1,480);
  table_->setColumnWidth(2,200);table_->setColumnWidth(3,240);
  table_->setColumnWidth(4,100);table_->setColumnWidth(5,300);
  table_->sortByColumn(1,Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  if(document_)updateDocumentBanner();
  else banner_->setText(QCoreApplication::translate(
      "RNet","No configuration has been read. Read the controller or open an R-net file."));
  QString details=documentPath_;
  if(!warnings_.empty())
    details+=QString(QCoreApplication::translate("RNet",
        "\n\n%1 file version(s) preserved as complete raw data, not yet interpreted as individual values."))
        .arg(warnings_.size());
  details_->setPlainText(details);
  if(preserveContext){
    search_->setText(query);
    for(QTreeWidgetItemIterator item(tree_);*item;++item)
      if((*item)->data(0,Qt::UserRole).toString()==group)tree_->setCurrentItem(*item);
    if(sortColumn>=0&&sortColumn<model_->columnCount())table_->sortByColumn(sortColumn,sortOrder);
    for(int row=0;row<model_->rowCount();++row)
      if(model_->rows[row].valueIndex==selectedValue&&selectedValue>=0){
        table_->setCurrentIndex(proxy_->mapFromSource(model_->index(row,2)));break;
      }
    table_->verticalScrollBar()->setValue(scroll);
  }
  updateActions();
}
void Window::updateDocumentBanner() {
  bool snapshot = false;
  for (const auto &c : document_->comments)
    if (c.rfind(u"CAN read backup;", 0) == 0)
      snapshot = true;
  banner_->setText(
      snapshot
          ? QCoreApplication::translate(
                "RNet", "Configuration backup read from the real CAN bus. All "
                        "received files are preserved; encrypted document "
                        "metadata comes from the original default template.")
          : QCoreApplication::translate(
                "RNet",
                "Stored raw values from an R-net document. Edits affect the "
                "local file. Unknown file versions are preserved unchanged."));
}
void Window::showValues() { renderValues(); }
void Window::showRepository() {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  view_ = View::Repository;
  if (!document_)
    return;
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  updateDocumentBanner();
  valuesView_ = false;
  std::vector<Row> rows;
  for (const auto &f : document_->files) {
    auto name = catalog_.file_names.find(ParameterCatalog::storage_id(f.id));
    rows.push_back({hex(f.id),
                    name == catalog_.file_names.end()
                        ? QCoreApplication::translate("RNet", "Repository file")
                        : fromU16(name->second),
                    QString::number(f.data.size()),
                    QString::number(f.version),
                    {},
                    QCoreApplication::translate(
                        "RNet", "Unchanged repository record; payloads and "
                                "unknown versions are preserved when saving."),
                    -1});
    rows.back().icon=Icon::Files;
  }
  model_->replace(std::move(rows),
                  {QCoreApplication::translate("RNet", "File ID"),
                   QCoreApplication::translate("RNet", "Repository file"),
                   "Bytes", "Version"});
  resetGroup(false);
  table_->setColumnHidden(2, false);
  table_->setColumnHidden(3, false);
  table_->setColumnWidth(0,130);table_->setColumnWidth(1,500);
  table_->setColumnWidth(2,100);table_->setColumnWidth(3,100);
  table_->sortByColumn(0,Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  details_->setPlainText(documentPath_);
  updateActions();
}
void Window::showResources() {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  view_ = View::Resources;
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  banner_->setText(QCoreApplication::translate(
      "RNet",
      "Bundled application text resources. Controller data is not changed."));
  details_->setPlainText(QCoreApplication::translate(
      "RNet", "Text resources embedded in dfreernet."));
  valuesView_ = false;
  std::vector<Row> rows;
  for (const auto &entry : bundled_text_resources()) {
    const auto text=fromU16(entry.text);
    rows.push_back({QString::number(entry.id), text, {}, {}, {}, text, -1});
  }
  model_->replace(std::move(rows),
                  {QCoreApplication::translate("RNet", "Resource ID"),
                   QCoreApplication::translate("RNet", "Resource text"), "",
                   ""});
  resetGroup(false);
  table_->setColumnHidden(2, true);
  table_->setColumnHidden(3, true);
  table_->setColumnWidth(0,130);table_->setColumnWidth(1,700);
  table_->sortByColumn(0,Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  updateActions();
}
void Window::openDocument(const QString &name) { loadDocument(name, false); }
void Window::loadDocument(const QString &name, bool replacementApproved) {
  auto bytes = read_file(path(name));
  auto doc = std::make_unique<RnetDocument>(RnetDocument::read(bytes));
  std::vector<std::string> warnings;
  auto values = read_values(*doc, catalog_, &warnings);
  if (!replacementApproved && !finishChanges())
    return;
  document_ = std::move(doc);
  values_ = std::move(values);
  warnings_ = std::move(warnings);
  saved_ = bytes;
  controllerBaseline_ = bytes;
  documentPath_ = QFileInfo(name).absoluteFilePath();
  owned_ = false;
  undo_.clear();
  renderValues();
  updateTitle();
  log(QString("DOCUMENT_OPEN path=%1 files=%2 values=%3 opaque=%4")
          .arg(documentPath_)
          .arg(fileCount())
          .arg(valueCount())
          .arg(warningCount()));
}
void Window::updateTitle() {
  setWindowTitle(
      (document_ ? QFileInfo(documentPath_).fileName() + "[*] — " : QString()) +
      "dfreernet");
  setWindowModified(document_ && document_->write() != saved_);
}
void Window::updateActions() {
  syncAccessColumn();
  const auto columnKey=columnLayoutKey();
  if(saveColumnsAction_)saveColumnsAction_->setEnabled(!columnKey.isEmpty());
  if(loadColumnsAction_)loadColumnsAction_->setEnabled(
      !columnKey.isEmpty()&&QSettings().contains(columnKey));
  if(resetColumnsAction_)resetColumnsAction_->setEnabled(!columnKey.isEmpty());
  int index = -1;
  auto current = table_->currentIndex();
  if (current.isValid()) {
    auto source = proxy_->mapToSource(current);
    if (source.isValid() && source.row() < int(model_->rows.size()))
      index = source.data(Qt::UserRole).toInt();
  }
  bool editable = false;
  if (valuesView_ && document_ && index >= 0 && index < int(values_.size())) {
    const auto &p = catalog_.parameters.at(values_[index].parameter);
    editable = access_allows(p.metadata[1] >> 5, access_) && supports_direct_edit(p,values_[index],&catalog_);
  }
  edit_->setEnabled(editable && !busy());
  undoButton_->setEnabled(!undo_.empty() && !busy());
  for (auto *role : roleActions_) role->setEnabled(!busy());
  if (actions_.count(32804)) actions_[32804]->setEnabled(document_ && !busy());
  for (unsigned id : {57603u, 57604u, 32939u, Values, Repository})
    if (actions_.count(id))
      actions_[id]->setEnabled(document_ && !busy());
  if (actions_.count(57643))
    actions_[57643]->setEnabled(!undo_.empty() && !busy());
  if (actions_.count(Edit))
    actions_[Edit]->setEnabled(edit_->isEnabled());
  if(parameterHelpAction_){
    const auto current=proxy_->mapToSource(table_->currentIndex());
    parameterHelpAction_->setEnabled(
        (view_==View::Values||view_==View::Catalog)&&current.isValid()&&
        current.row()<int(model_->rows.size())&&
        catalog_.parameters.count(model_->rows[std::size_t(current.row())].id.toUInt()));
  }
  for (unsigned id : {57601u, 32803u, 40030u, 40031u, 40025u, 40002u, 40040u})
    if (actions_.count(id))
      actions_[id]->setEnabled(!busy());
  accessLabel_->setText(QCoreApplication::translate("RNet","Dongle: %1 · %2").arg(unsigned(access_)).arg(QCoreApplication::translate("RNet",access_level_name(access_))));
  accessLabel_->setStyleSheet(QString("QLabel {background-color:%1;color:%2;border-radius:3px;padding:2px 8px;font-weight:600;}").arg(accessColor(int(access_)).name(),unsigned(access_)>=2?QStringLiteral("white"):QStringLiteral("black")));
  accessLabel_->setToolTip(QCoreApplication::translate("RNet","Access level"));
  statusBar()->showMessage(
      QString(QCoreApplication::translate(
                  "RNet", "%1 | %2 visible entries%3"))
          .arg(document_
                   ? QString(QCoreApplication::translate(
                                 "RNet", "%1 values; %2 unknown versions"))
                         .arg(valueCount())
                         .arg(warningCount())
                   : QCoreApplication::translate("RNet",
                                                 "No configuration read"))
          .arg(proxy_->rowCount())
          .arg(busy()
                   ? QCoreApplication::translate("RNet", " | CAN task running")
                   : QCoreApplication::translate(
                         "RNet", " | Controller not connected")));
}
void Window::syncAccessColumn(){
  if(!accessTable_)return;
  const bool show=(view_==View::Values||view_==View::Catalog)&&model_->columnCount()>0;
  const int headerHeight=table_->horizontalHeader()->fontMetrics().lineSpacing()*2+12;
  table_->horizontalHeader()->setFixedHeight(headerHeight);
  accessTable_->horizontalHeader()->setFixedHeight(headerHeight);
  accessTable_->setVisible(show);
  if(!show)return;
  const int accessColumn=model_->columnCount()-1;
  for(int column=0;column<model_->columnCount();++column)
    accessTable_->setColumnHidden(column,column!=accessColumn);
  table_->setColumnHidden(accessColumn,true);
  accessTable_->setColumnWidth(accessColumn,160);
  accessTable_->verticalScrollBar()->setValue(table_->verticalScrollBar()->value());
}
QString Window::columnLayoutKey() const{
  QString mode;
  switch(view_){
  case View::Values:
    mode=settings_.profileTableView?QStringLiteral("profiles"):
        settings_.moduleGroupView?QStringLiteral("modules"):QStringLiteral("hierarchy");
    break;
  case View::Catalog:mode=QStringLiteral("catalog");break;
  case View::Repository:mode=QStringLiteral("repository");break;
  case View::Resources:mode=QStringLiteral("resources");break;
  case View::Monitor:mode=QStringLiteral("monitor");break;
  }
  return QStringLiteral("ui/columns/v1/")+mode;
}
void Window::resetColumnOrder(){
  auto *header=table_->horizontalHeader();
  for(int logical=0;logical<model_->columnCount();++logical)
    header->moveSection(header->visualIndex(logical),logical);
}
void Window::restoreSavedColumns(){
  const auto key=columnLayoutKey();
  if(key.isEmpty())return;
  const auto state=pendingColumnKey_==key?pendingColumnState_:
      QSettings().value(key).toByteArray();
  if(!state.isEmpty())table_->horizontalHeader()->restoreState(state);
}
void Window::queueColumnSave(){
  if(!columnLayoutReady_||columnLayoutBusy_)return;
  pendingColumnKey_=columnLayoutKey();
  pendingColumnState_=table_->horizontalHeader()->saveState();
  columnSaveTimer_->start();
}
void Window::flushColumnSave(){
  if(pendingColumnKey_.isEmpty())return;
  if(columnSaveTimer_)columnSaveTimer_->stop();
  QSettings settings;
  settings.setValue(pendingColumnKey_,pendingColumnState_);
  settings.sync();
  if(settings.status()!=QSettings::NoError){
    log("COLUMN_LAYOUT_SAVE_FAILED "+pendingColumnKey_);
    return;
  }
  pendingColumnKey_.clear();
  pendingColumnState_.clear();
  if(saveColumnsAction_)updateActions();
}
void Window::saveColumns(){
  const auto key=columnLayoutKey();
  if(key.isEmpty())return;
  columnSaveTimer_->stop();
  pendingColumnKey_.clear();pendingColumnState_.clear();
  QSettings settings;
  settings.setValue(key,table_->horizontalHeader()->saveState());
  settings.sync();
  if(settings.status()!=QSettings::NoError)
    throw UiError(QCoreApplication::translate("RNet","Could not save column layout"));
  updateActions();
}
void Window::loadColumns(){
  const auto key=columnLayoutKey();
  if(key.isEmpty())return;
  columnSaveTimer_->stop();
  pendingColumnKey_.clear();pendingColumnState_.clear();
  const auto state=QSettings().value(key).toByteArray();
  if(state.isEmpty()||!table_->horizontalHeader()->restoreState(state))
    throw UiError(QCoreApplication::translate("RNet","No valid saved column layout"));
  syncAccessColumn();
  updateActions();
}
void Window::resetColumns(){
  const auto key=columnLayoutKey();
  if(key.isEmpty())return;
  columnSaveTimer_->stop();
  pendingColumnKey_.clear();pendingColumnState_.clear();
  QSettings settings;
  settings.remove(key);
  settings.sync();
  if(settings.status()!=QSettings::NoError)
    throw UiError(QCoreApplication::translate("RNet","Could not reset column layout"));
  switch(view_){
  case View::Values:renderValues(true);break;
  case View::Catalog:showCatalog();break;
  case View::Repository:showRepository();break;
  case View::Resources:showResources();break;
  case View::Monitor:showMonitorCapture(false);break;
  }
}
void Window::selected() {
  auto current = proxy_->mapToSource(table_->currentIndex());
  if (current.isValid() && current.row() < int(model_->rows.size()))
    details_->setPlainText(current.data(Qt::ToolTipRole).toString());
  updateActions();
}
void Window::showParameterHelp() {
  const auto current=proxy_->mapToSource(table_->currentIndex());
  if(!current.isValid()||current.row()>=int(model_->rows.size())||
      (view_!=View::Values&&view_!=View::Catalog))return;
  const auto &row=model_->rows[std::size_t(current.row())];
  bool valid=false;
  const unsigned id=row.id.toUInt(&valid);
  if(!valid)return;
  const auto found=catalog_.parameters.find(id);
  if(found==catalog_.parameters.end())return;
  const auto &parameter=found->second;
  QDialog dialog(this);
  dialog.setObjectName("parameterHelpDialog");
  dialog.setWindowTitle(QCoreApplication::translate("RNet","Parameter help — %1").arg(id));
  dialog.resize(650,420);
  auto *layout=new QVBoxLayout(&dialog);
  auto *heading=new QLabel(QStringLiteral("<b>%1</b> · ID %2")
      .arg(fromU16(parameter.name()).toHtmlEscaped()).arg(id));
  heading->setTextFormat(Qt::RichText);
  heading->setWordWrap(true);
  layout->addWidget(heading);
  auto *context=new QLabel(
      QCoreApplication::translate("RNet","Group: %1\nProfile / element: %2\nStored raw value: %3\nAccess level: %4")
        .arg(fromU16(parameter.text88))
        .arg(settings_.profileTableView&&view_==View::Values&&tree_->currentItem()?
            tree_->currentItem()->text(0)+" / "+row.location:
            (row.location.isEmpty()?QStringLiteral("—"):row.location))
        .arg(row.unread?QCoreApplication::translate("RNet","Not read"):row.value)
        .arg(QCoreApplication::translate("RNet",access_level_name(
            AccessLevel(parameter.metadata[1]>>5)))));
  context->setObjectName("parameterHelpContext");
  context->setWordWrap(true);
  layout->addWidget(context);
  auto *help=new QPlainTextEdit;
  help->setObjectName("parameterHelpText");
  help->setReadOnly(true);
  const auto description=fromU16(parameter.help()).trimmed();
  help->setPlainText(description.isEmpty()?
      QCoreApplication::translate("RNet","No parameter help text is available in the bundled catalog."):
      description);
  layout->addWidget(help,1);
  auto *source=new QLabel(QCoreApplication::translate(
      "RNet","Source: bundled R-net parameter catalog. The R-net PC Programmer guide, section 3.3.2, describes this parameter help text."));
  source->setWordWrap(true);
  layout->addWidget(source);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
}
void Window::showAbout(){
  QDialog dialog(this);
  dialog.setObjectName("aboutDialog");
  dialog.setWindowTitle(QCoreApplication::translate("RNet","About dfreernet"));
  dialog.setMinimumWidth(540);
  auto *layout=new QVBoxLayout(&dialog);
  auto *heading=new QLabel(QStringLiteral("<h2>dfreernet %1</h2>%2")
      .arg(QStringLiteral(DFREERNET_VERSION),
           QCoreApplication::translate("RNet","Dongle free R-Net Programmer")));
  heading->setObjectName("aboutHeading");
  heading->setTextFormat(Qt::RichText);
  layout->addWidget(heading);
  auto *description=new QLabel(QCoreApplication::translate("RNet",
      "Native C++17 / Qt 6 application for Linux with SocketCAN. Read controller configuration, "
      "edit supported parameters, save working copies, and write supported changes with "
      "readback verification.\n\nExperimental; rudimentarily tested on Permobil T5 VS. "
      "Compatibility with other wheelchairs or firmware versions has not been established. "
      "Additional file versions, parameter conversions, and encrypted document metadata "
      "generation remain incomplete."));
  description->setWordWrap(true);
  layout->addWidget(description);
  const QString email=QStringLiteral("JSievers@NadiSoft.de");
  const QString repository=QStringLiteral("https://github.com/notwendig/dfreernet");
  auto *contact=new QLabel(QStringLiteral("<b>%1</b><br><a href=\"mailto:%2\">%2</a><br><a href=\"%3\">%3</a>")
      .arg(QStringLiteral("Jürgen Willi Sievers"),email,repository));
  contact->setObjectName("aboutContact");
  contact->setTextFormat(Qt::RichText);
  contact->setTextInteractionFlags(Qt::TextBrowserInteraction);
  contact->setOpenExternalLinks(true);
  layout->addWidget(contact);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
}
void Window::setAccess(AccessLevel level) {
  if (unsigned(level) > 3)
    throw UiError(QCoreApplication::translate("RNet", "Invalid access level"));
  commitPendingValueEditor();
  access_ = level;
  if(unsigned(level)<2){settings_.accessLevel=int(level);QSettings().setValue("dongle/accessLevel",int(level));}
  proxy_->access = level;
  const auto query = search_->text();
  if (view_ == View::Values || view_ == View::Catalog) {
    if(view_==View::Values&&settings_.profileTableView)
      buildProfileGroups(proxy_->group);
    else if(view_==View::Values&&settings_.moduleGroupView)
      buildModuleGroups(proxy_->group);
    else{
      buildGroups();
      proxy_->group.clear();
    }
    search_->setText(query);
  }
  proxy_->refilter();
  for (unsigned i = 0; i < roleActions_.size(); ++i)
    roleActions_[i]->setChecked(i == unsigned(level));
  updateActions();
  log("DONGLE_ACCESS_SELECTED " +
      QString::fromLatin1(access_level_name(level)) + "; display/edit/write ceiling");
}
void Window::applyEdit(int index, const QString &input) {
  if (busy() || !document_ || index < 0 || index >= int(values_.size()))
    throw UiError(
        QCoreApplication::translate("RNet", "No editable value selected"));
  auto before = document_->write();
  if (!edit_value(*document_, catalog_, values_[index], input.toStdU16String(),
                  access_))
    return;
  undo_.push_back(std::move(before));
  if (undo_.size() > std::size_t(settings_.undoLimit))
    undo_.erase(undo_.begin());
  warnings_.clear();
  values_ = read_values(*document_, catalog_, &warnings_);
  renderValues(true);
  updateTitle();
  log("DOCUMENT_VALUE_EDIT local only");
}
void Window::commitPendingValueEditor() {
  ParameterValueEditor *editor=nullptr;
  for(auto *widget:table_->findChildren<QWidget*>("parameterValueEditor"))
    if(auto *candidate=dynamic_cast<ParameterValueEditor*>(widget))
      if(!candidate->property("committing").toBool()){editor=candidate;break;}
  if(!editor)return;
  QString error;if(!editor->valid(&error))throw UiError(error);
  const int valueIndex=editor->property("valueIndex").toInt();
  const auto input=editor->rawValue();editor->setProperty("committing",true);
  QPointer<ParameterValueEditor> guard(editor);
  try{applyEdit(valueIndex,input);}catch(...){if(guard)guard->setProperty("committing",false);throw;}
  // Unchanged values do not reset the model; close their editor explicitly.
  if(guard)guard->deleteLater();
}
void Window::editSelected() {
  commitPendingValueEditor();
  if (!edit_->isEnabled())
    return;
  auto source = proxy_->mapToSource(table_->currentIndex());
  if (!source.isValid())
    return;
  int index = source.data(Qt::UserRole).toInt();
  if (index < 0)
    return;
  const auto &v = values_.at(index);
  const auto &p = catalog_.parameters.at(v.parameter);
  QString prompt = fromU16(p.name()) +
                   QCoreApplication::translate("RNet", "\nStored raw value; ");
  if (v.numeric)
    prompt += QString(QCoreApplication::translate("RNet", "range %1 to %2"))
                  .arg(p.signed_storage ? qint64(qint32(p.metadata[4]))
                                        : p.metadata[4])
                  .arg(p.signed_storage ? qint64(qint32(p.metadata[5]))
                                        : p.metadata[5]);
  else
    prompt +=
        QString(QCoreApplication::translate("RNet", "up to %1 characters"))
            .arg(v.width);
  QDialog dialog(this);dialog.setObjectName("parameterValueDialog");
  dialog.setWindowTitle(QCoreApplication::translate("RNet","Edit value in document"));
  auto *layout=new QVBoxLayout(&dialog);
  auto *description=new QLabel(prompt);description->setWordWrap(true);layout->addWidget(description);
  auto *editor=new ParameterValueEditor(p,v,catalog_,&dialog);layout->addWidget(editor);
  auto *error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color: red");layout->addWidget(error);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  auto validate=[editor,error,buttons]{QString reason;bool valid=editor->valid(&reason);error->setText(reason);buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);};
  editor->validityChanged=validate;validate();
  connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  dialog.resize(520,180);editor->setFocus();
  if(dialog.exec()==QDialog::Accepted&&editor->valid())applyEdit(index,editor->rawValue());
}
void Window::undo() {
  if(!busy())commitPendingValueEditor();
  if (busy() || undo_.empty())
    return;
  auto next = RnetDocument::read(undo_.back());
  std::vector<std::string> warnings;
  auto values = read_values(next, catalog_, &warnings);
  document_ = std::make_unique<RnetDocument>(std::move(next));
  undo_.pop_back();
  values_ = std::move(values);
  warnings_ = std::move(warnings);
  renderValues(true);
  updateTitle();
  log("DOCUMENT_UNDO");
}
void Window::saveAs(const QString &name) {
  commitPendingValueEditor();
  if (!document_)
    throw UiError(QCoreApplication::translate("RNet", "No document is open"));
  QFileInfo target(name);
  if (target.suffix().compare("r-net", Qt::CaseInsensitive) != 0)
    throw UiError(QCoreApplication::translate(
        "RNet", "The .R-net file extension is required"));
  auto bytes = document_->write();
  writeNew(target.absoluteFilePath(), bytes);
  saved_ = bytes;
  documentPath_ = target.absoluteFilePath();
  owned_ = true;
  updateTitle();
  log("DOCUMENT_SAVED_AS " + documentPath_);
}
void Window::save() {
  commitPendingValueEditor();
  if (!document_)
    return;
  if (!owned_) {
    selectSaveAs();
    return;
  }
  if (read_file(path(documentPath_)) != saved_)
    throw UiError(QCoreApplication::translate(
        "RNet", "The file was changed externally. Save it under a new name."));
  auto bytes = document_->write();
  QSaveFile f(documentPath_);
  f.setDirectWriteFallback(false);
  if (!f.open(QIODevice::WriteOnly) ||
      f.write(array(bytes)) != qsizetype(bytes.size()) || !f.commit())
    throw UiError(QCoreApplication::translate(
        "RNet", "Could not atomically save the working copy"));
  saved_ = bytes;
  updateTitle();
  log("DOCUMENT_SAVED working copy");
}
bool Window::finishChanges() {
  commitPendingValueEditor();
  if (!document_ || document_->write() == saved_)
    return true;
  auto answer = QMessageBox::question(
      this, QCoreApplication::translate("RNet", "Unsaved changes"),
      QCoreApplication::translate("RNet",
                                  "Save changed parameters to a working copy?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Cancel);
  if (answer == QMessageBox::Cancel)
    return false;
  if (answer == QMessageBox::Save) {
    save();
    return document_->write() == saved_;
  }
  return true;
}
void Window::closeDocument() {
  if (!finishChanges())
    return;
  document_.reset();
  values_.clear();
  warnings_.clear();
  undo_.clear();
  saved_.clear();
  documentPath_.clear();
  owned_ = false;
  showValues();
  updateTitle();
}
void Window::selectOpen() {
  auto name = QFileDialog::getOpenFileName(
      this, QCoreApplication::translate("RNet", "Open R-net document"),
      documentPath_.isEmpty() ? settings_.workingDirectory : documentPath_,
      QCoreApplication::translate(
          "RNet", "R-net documents (*.R-net *.r-net);;All files (*)"));
  if (!name.isEmpty())
    openDocument(name);
}
void Window::selectSaveAs() {
  QFileInfo source(documentPath_);
  QDir().mkpath(settings_.workingDirectory);
  auto proposed = QDir(settings_.workingDirectory).filePath(
      source.completeBaseName() +
      QCoreApplication::translate("RNet", "-WorkingCopy.R-net"));
  auto name = QFileDialog::getSaveFileName(
      this,
      QCoreApplication::translate("RNet", "Write configuration to a new file"),
      proposed,
      QCoreApplication::translate("RNet", "R-net documents (*.R-net *.r-net)"),
      nullptr, QFileDialog::DontConfirmOverwrite);
  if (!name.isEmpty()) {
    if (QFileInfo(name).suffix().isEmpty())
      name += ".R-net";
    saveAs(name);
  }
}
void Window::exportCsv(const QString &name) {
  QFile f(name);
  if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    throw UiError(QCoreApplication::translate(
        "RNet", "The CSV destination already exists or is not writable"));
  QTextStream stream(&f);
  QStringList headings;
  for (const auto &h : model_->headings)
    headings << csv(h);
  stream << headings.join(';') << '\n';
  for (int row = 0; row < proxy_->rowCount(); ++row) {
    QStringList fields;
    for (int col = 0; col < proxy_->columnCount(); ++col)
      fields << csv(proxy_->index(row, col).data().toString());
    stream << fields.join(';') << '\n';
  }
  stream.flush();
  if (stream.status() != QTextStream::Ok || !f.flush())
    throw UiError(QCoreApplication::translate(
        "RNet", "Could not write the complete CSV file"));
  log("CSV_EXPORTED " + name);
}
void Window::selectExport() {
  if(!QDir().mkpath(settings_.exportDirectory))throw UiError(QCoreApplication::translate("RNet","Cannot create export directory"));
  auto name = QFileDialog::getSaveFileName(
      this, QCoreApplication::translate("RNet", "Export visible rows"),
      QDir(settings_.exportDirectory).filePath("rnet-view.csv"), "CSV (*.csv)", nullptr,
      QFileDialog::DontConfirmOverwrite);
  if (!name.isEmpty())
    exportCsv(name);
}
void Window::decodeRnd() {
  blinkCycle_->quiesce();
  auto name=QFileDialog::getOpenFileName(this,QCoreApplication::translate("RNet","Import RND catalog"),settings_.workingDirectory,"RND (*.rnd *.RND)");
  if(name.isEmpty())return;
  const auto plain=RndDecoder().decode(read_file(path(name)));
  const auto imported=ParameterCatalog::read(plain);
  if(!QDir().mkpath(settings_.exportDirectory))throw UiError(QCoreApplication::translate("RNet","Cannot create export directory"));
  auto target=QFileDialog::getSaveFileName(this,QCoreApplication::translate("RNet","Save imported SQLite catalog"),QDir(settings_.exportDirectory).filePath(QFileInfo(name).completeBaseName()+".sqlite3"),"SQLite (*.sqlite3 *.sqlite)",nullptr,QFileDialog::DontConfirmOverwrite);
  if(target.isEmpty())return;
  if(QFileInfo(target).suffix().isEmpty())target+=".sqlite3";
  write_catalog_database(plain,path(target));
  log("RND_IMPORTED_SQLITE "+target);
  QMessageBox::information(this,"dfreernet",QCoreApplication::translate("RNet","Imported %1 parameters, %2 storage maps and %3 editors into SQLite. The built-in catalog and controller data are unchanged.\n\n%4").arg(imported.parameters.size()).arg(imported.storage_maps.size()).arg(imported.editors.size()).arg(target));
}

void Window::command(unsigned id) {
  if (busy() && id != 57665 && id != 57636)
    return;
  commitPendingValueEditor();
  log("COMMAND " + QString::number(id));
  switch (id) {
  case 57665:
    close();
    break;
  case 57601:
    selectOpen();
    break;
  case 57603:
    save();
    break;
  case 57604:
    selectSaveAs();
    break;
  case 57643:
    undo();
    break;
  case 32939:
    closeDocument();
    break;
  case 57636:
    search_->setFocus();
    search_->selectAll();
    break;
  case 32844:
    tree_->expandAll();
    break;
  case 32831:
    tree_->collapseAll();
    tree_->topLevelItem(0)->setExpanded(true);
    break;
  case Values:
    showValues();
    break;
  case Catalog:
    showCatalog();
    break;
  case Repository:
    showRepository();
    break;
  case Resources:
    showResources();
    break;
  case Edit:
    editSelected();
    break;
  case Export:
    selectExport();
    break;
  case 40040:
    openSettings();
    break;
  case 40002:
    decodeRnd();
    break;
  case 40003:
    hardwareInfo();
    break;
  case 40025: {
    auto interface = selectInterface();
    if (!interface.isEmpty())
      startMonitor(interface,settings_.monitorSeconds*1000);
    break;
  }
  case 32804:
    requestControllerWrite();
    break;
  case 32803:
  case 40030:
    requestConfiguration();
    break;
  case 40031:
    requestIdentities();
    break;
  case 57664:
    showAbout();
    break;
  default:
    break;
  }
}
void Window::updateBlinkState() {
  const bool active = blinkCycle_->active();
  const auto error = blinkCycle_->error();
  blinkAction_->setChecked(active);
  blinkLabel_->setVisible(active || !error.isEmpty());

  const auto icon = error.isEmpty() ? Icon::Light : Icon::Stop;
  const auto mode = error.isEmpty() && !blinkCycle_->pending() ? QIcon::Disabled : QIcon::Normal;
  blinkLabel_->setPixmap(uiIcon(icon).pixmap(QSize(24, 24), blinkLabel_->devicePixelRatioF(), mode));

  QString status = error;
  if (status.isEmpty())
    status = QCoreApplication::translate("RNet", "Light signal every %1 s for %2 s")
                 .arg(blinkCycle_->intervalMs() / 1000)
                 .arg(blinkCycle_->durationMs() / 1000);
  blinkLabel_->setToolTip(status);
  blinkLabel_->setAccessibleName(status);
}
bool Window::heartbeatReceiving()const{return heartbeat_&&heartbeat_->receiving();}
void Window::startBlinkCycle(){
  if(blinkCycle_->active())return;
  if(busy())throw UiError(QCoreApplication::translate("RNet","A CAN task is already running"));
  if(!heartbeatReceiving())throw UiError(QCoreApplication::translate("RNet","No current CAN heartbeat; blink test not started"));
  if(QMessageBox::question(this,QCoreApplication::translate("RNet","Live signal: hazard lights"),
      QCoreApplication::translate("RNet","Are the hazard lights OFF and the wheelchair stationary? This CAN command only toggles them. Starting while they are ON will leave them ON between pulses. The cycle does not prevent automatic power-off."),
      QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes){
    settings_.liveSignalEnabled=false;return;
  }
  blinkCycle_->configure(settings_.liveSignalIntervalSeconds * 1000, settings_.liveSignalDurationSeconds * 1000);
  blinkCycle_->start(settings_.canInterface);settings_.liveSignalEnabled=blinkCycle_->active();
}
void Window::requestBlinkCycle(){startBlinkCycle();}
void Window::updateSleepStatus(){
  sleepLabel_->setText(QCoreApplication::translate("RNet",sleepInhibitor_->sleepBlocked()?"Linux awake":"Sleep lock"));sleepLabel_->setVisible(sleepInhibitor_->requested());sleepLabel_->setEnabled(sleepInhibitor_->sleepBlocked()||sleepInhibitor_->idleBlocked());
  const auto status=sleepInhibitor_->statusText();sleepLabel_->setToolTip(status);sleepLabel_->setAccessibleName(status);
  if(status!=sleepStatus_){sleepStatus_=status;log("SLEEP_INHIBITOR "+status);}
}
void Window::applyPreferences(){
  if(blinkCycle_->active()&&(blinkCycle_->interface()!=settings_.canInterface||!settings_.liveSignalEnabled))blinkCycle_->stop();
  blinkCycle_->configure(settings_.liveSignalIntervalSeconds * 1000, settings_.liveSignalDurationSeconds * 1000);
  sleepInhibitor_->configure(settings_.keepAwake);updateSleepStatus();
  applyAppFont(settings_.fontSize);
  const auto toolbarStyle = settings_.toolbarIconsOnly ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
  for (auto *toolbar : findChildren<QToolBar *>()) {
    toolbar->setToolButtonStyle(toolbarStyle);
    toolbar->setIconSize(QSize(settings_.toolbarIconSize, settings_.toolbarIconSize));
  }
  tree_->setIconSize(QSize(settings_.itemIconSize,settings_.itemIconSize));table_->setIconSize(QSize(settings_.itemIconSize,settings_.itemIconSize));
  detailsAction_->setChecked(settings_.showDetails);details_->setVisible(settings_.showDetails);
  heartbeat_->configure(settings_.canInterface,settings_.heartbeatEnabled,settings_.heartbeatTimeoutMs);heartbeat_->setVisible(settings_.heartbeatEnabled);
}
void Window::openSettings(){
  if(busy())return;
  commitPendingValueEditor();auto old=settings_;auto initial=settings_;initial.liveSignalEnabled=blinkCycle_->active();initial.language=languages().language();
  SettingsDialog dialog(initial,this);
  dialog.applied=[this](const AppSettings &s){
    const bool startRequested=s.liveSignalEnabled&&!blinkCycle_->active();
    const bool accessChanged=settings_.accessLevel!=s.accessLevel;
    settings_=s;if(accessChanged)setAccess(AccessLevel(s.accessLevel));
    if(undo_.size()>std::size_t(s.undoLimit))undo_.erase(undo_.begin(),undo_.end()-s.undoLimit);
    applyPreferences();
    if(startRequested)guarded([this]{startBlinkCycle();});
    if(view_==View::Values)renderValues(true);
    updateActions();
  };
  dialog.exec();
  if(old.monitor!=settings_.monitor||old.maximized!=settings_.maximized)placeOnSecondary();
}
void Window::placeOnSecondary() {
  auto *screen=preferredScreen(settings_.monitor);
  if (!screen)
    return;
  auto area = screen->availableGeometry();
  if(isMaximized()||isFullScreen())showNormal();
  winId();windowHandle()->setScreen(screen);
  resize(qMin(1200,area.width()-32),qMin(820,area.height()-64));
  move(area.center()-rect().center());
  if(settings_.maximized)showMaximized();else showNormal();
  log(QString("SCREEN %1 primary=%2 geometry=%3,%4,%5,%6")
          .arg(screen->name())
          .arg(screen == QGuiApplication::primaryScreen())
          .arg(area.x())
          .arg(area.y())
          .arg(area.width())
          .arg(area.height()));
}
QString Window::selectInterface() {
  return settings_.canInterface;
}
void Window::requestConfiguration() {
  if (busy())
    return;
  auto interface = selectInterface();
  if (interface.isEmpty())
    return;
  QDialog dialog(this);
  dialog.setWindowTitle(QCoreApplication::translate(
      "RNet", "Request and read configuration only"));
  auto *layout = new QVBoxLayout(&dialog);
  auto *text = new QLabel(QCoreApplication::translate(
      "RNet", "All available files in the known catalog will be requested. "
              "Programming access may temporarily block driving. The "
              "configuration will remain unchanged."));
  text->setWordWrap(true);
  layout->addWidget(text);
  auto *target=new QLabel(QString(QCoreApplication::translate("RNet","Interface: %1\nBackup directory: %2")).arg(interface,settings_.backupDirectory));target->setWordWrap(true);layout->addWidget(target);
  auto *stationary = new QCheckBox(QCoreApplication::translate(
      "RNet", "The wheelchair is stationary; driving is not needed"));
  layout->addWidget(stationary);
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)
      ->setText(QCoreApplication::translate("RNet", "Read only"));
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  connect(stationary, &QCheckBox::toggled,
          buttons->button(QDialogButtonBox::Ok), &QWidget::setEnabled);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted)
    return;
  startControllerRead(interface,settings_.newBackupDirectory("rnet-config"),true);
}
void Window::startControllerRead(const QString &interface,
                                 const QString &destination,
                                 bool stationaryConfirmed) {
  if (!stationaryConfirmed)
    throw UiError(QCoreApplication::translate(
        "RNet", "Stationary operation must be confirmed before reading"));
  if (busy())
    throw UiError(
        QCoreApplication::translate("RNet", "A CAN task is already running"));
  if (interface.isEmpty() || destination.isEmpty() ||
      QFileInfo::exists(destination))
    throw UiError(QCoreApplication::translate(
        "RNet", "A CAN interface and a new backup folder are required"));
  if (!finishChanges())
    return;
  taskDirectory_ = QFileInfo(destination).absoluteFilePath();
  startTool("rnet_config_read", {interface, taskDirectory_, "read"}, true);
}
void Window::requestControllerWrite() {
  commitPendingValueEditor();
  if (busy() || !document_) return;
  auto before=RnetDocument::read(controllerBaseline_);
  const auto files=controller_write_plan(before,*document_,catalog_,access_);
  if (files.empty()) throw UiError(QCoreApplication::translate("RNet", "No parameter changes to write"));
  QString preview;
  std::vector<std::string> warnings;
  auto baselineValues=read_values(before,catalog_,&warnings);
  for (std::size_t i=0;i<values_.size();++i) {
    const auto &v=values_[i], &old=baselineValues.at(i);
    if (v.number==old.number && v.text==old.text) continue;
    preview += QString::number(v.parameter) + " · " + fromU16(catalog_.parameters.at(v.parameter).name()) +
        " [" + QString::number(v.profile+1) + "/" + QString::number(v.element+1) + "]: " +
        (v.numeric ? QString::number(old.number) : fromU16(old.text)) + " → " +
        (v.numeric ? QString::number(v.number) : fromU16(v.text)) + '\n';
  }
  auto interface=selectInterface();if(interface.isEmpty()) return;
  QDialog dialog(this);
  dialog.setWindowTitle(QCoreApplication::translate("RNet", "Write to controller"));
  auto *layout=new QVBoxLayout(&dialog);
  auto *description=new QLabel(QCoreApplication::translate("RNet",
      "The listed changes will be written using the selected Dongle access level. A fresh comparison, backup and complete readback of each affected file are required."));
  description->setWordWrap(true);layout->addWidget(description);
  auto *target=new QLabel(QString(QCoreApplication::translate("RNet","Interface: %1\nBackup directory: %2")).arg(interface,settings_.backupDirectory));target->setWordWrap(true);layout->addWidget(target);
  auto *changes=new QPlainTextEdit(preview);changes->setReadOnly(true);layout->addWidget(changes);
  auto *stationary=new QCheckBox(QCoreApplication::translate("RNet",
      "The wheelchair is stationary; driving is not needed until readback finishes"));
  layout->addWidget(stationary);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("RNet", "Write and verify"));
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  connect(stationary,&QCheckBox::toggled,buttons->button(QDialogButtonBox::Ok),&QWidget::setEnabled);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  layout->addWidget(buttons);dialog.resize(660,420);
  if(dialog.exec()!=QDialog::Accepted) return;
  startControllerWrite(interface,settings_.newBackupDirectory("rnet-write"),true);
}
void Window::startControllerWrite(const QString &interface,const QString &destination,bool stationaryConfirmed) {
  if(!stationaryConfirmed) throw UiError(QCoreApplication::translate("RNet", "Stationary operation must be confirmed before writing"));
  if(busy() || !document_) throw UiError(QCoreApplication::translate("RNet", "No idle configuration to write"));
  commitPendingValueEditor();
  auto before=RnetDocument::read(controllerBaseline_);
  auto files=controller_write_plan(before,*document_,catalog_,access_);
  if(files.empty()) throw UiError(QCoreApplication::translate("RNet", "No parameter changes to write"));
  if(interface.isEmpty() || destination.isEmpty() || QFileInfo::exists(destination))
    throw UiError(QCoreApplication::translate("RNet", "A CAN interface and a new backup folder are required"));
  taskDirectory_=QFileInfo(destination).absoluteFilePath();
  if(!QDir().mkpath(taskDirectory_)) throw UiError(QCoreApplication::translate("RNet", "Cannot create the log directory"));
  writeNew(taskDirectory_+"/before.R-net",controllerBaseline_);
  writeNew(taskDirectory_+"/desired.R-net",document_->write());
  startTool("rnet_program_controller",{interface,taskDirectory_+"/before.R-net",taskDirectory_+"/desired.R-net",taskDirectory_+"/output","--stationary-confirmed","--access-level="+QString::number(unsigned(access_))},false);
}
void Window::requestIdentities() {
  if (busy())
    return;
  auto interface = selectInterface();
  if (!interface.isEmpty())
    startTool("rnet_serial_query", {interface}, false);
}
void Window::startTool(const QString &tool, const QStringList &args,
                       bool config) {
  if(blinkCycle_)blinkCycle_->quiesce();
  if (busy())
    throw UiError(
        QCoreApplication::translate("RNet", "A CAN task is already running"));
  auto executable = QCoreApplication::applicationDirPath() + "/" + tool;
  if (!QFileInfo::exists(executable))
    throw UiError(QCoreApplication::translate(
        "RNet", "The native CAN read tool is missing"));
  process_ = new QProcess(this);
  process_->setProcessChannelMode(QProcess::MergedChannels);
  configTask_ = config;
  writeTask_ = tool == "rnet_program_controller";
  cancelRequested_ = false;
  processText_.clear();
  taskLog_->clear();
  taskPanel_->show();
  cancel_->setEnabled(true);
  cancel_->setText(
      config ? QCoreApplication::translate("RNet", "Cancel / release access")
             : QCoreApplication::translate("RNet", "Cancel"));
  connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
    auto text = QString::fromUtf8(process_->readAllStandardOutput());
    processText_ += text;
    taskLog_->moveCursor(QTextCursor::End);
    taskLog_->insertPlainText(text);
  });
  connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus status) {
            guarded([this, code, status] { processFinished(code, status); });
          });
  connect(process_, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              auto message = process_->errorString();
              process_->deleteLater();
              process_ = nullptr;
              cancel_->setEnabled(false);
              updateActions();
              QMessageBox::warning(
                  this, QCoreApplication::translate("RNet", "CAN read"),
                  message);
            }
          });
  connect(process_, &QProcess::started, this, [this] {
    log("CAN_TOOL_PID " + QString::number(process_->processId()));
    if (cancelRequested_ && process_->processId() > 0)
      ::kill(pid_t(process_->processId()), SIGINT);
  });
  process_->start(executable, args);
  log("CAN_TOOL_START " + tool + " " + args.join(' '));
  updateActions();
}
void Window::processFinished(int code, QProcess::ExitStatus status) {
  processText_ += QString::fromUtf8(process_->readAllStandardOutput());
  process_->deleteLater();
  process_ = nullptr;
  cancel_->setEnabled(false);
  updateActions();
  log(QString("CAN_TOOL_FINISHED code=%1 cancelled=%2")
          .arg(code)
          .arg(cancelRequested_));
  QFile file(output_ + "/can-tool-" +
             QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") +
             ".log");
  if (file.open(QIODevice::WriteOnly))
    file.write(processText_.toUtf8());
  if (writeTask_) {
    writeTask_ = false;
    if (processText_.contains("COMMIT_ACKNOWLEDGED_READBACK_PENDING"))
      throw UiError(QCoreApplication::translate("RNet",
          "The controller acknowledged writing, but readback is not yet verified. The changes may already be active. Do not write again; read the controller to verify."));
    if (code != 0 || status != QProcess::NormalExit ||
        !processText_.contains("COMPLETE write verified_files=") ||
        !processText_.contains("RELEASE_VERIFIED node=2 owner_status=0"))
      throw UiError(QCoreApplication::translate("RNet",
          "Controller write was not fully verified. Read the controller before retrying; inspect the backup and log."));
    loadDocument(taskDirectory_ + "/output/verified.R-net", true);
    taskLog_->appendPlainText(QCoreApplication::translate("RNet", "Controller changes written, read back and access released."));
    return;
  }
  if (cancelRequested_) {
    taskLog_->appendPlainText(QCoreApplication::translate(
        "RNet",
        "Task ended. Raw data already read remains in the backup folder."));
    return;
  }
  if (code != 0 || status != QProcess::NormalExit)
    throw UiError(QCoreApplication::translate(
        "RNet", "CAN query failed; see the log in the task area"));
  if (configTask_) {
    if (!processText_.contains("owner_status=0") ||
        !processText_.contains("COMPLETE mode=read"))
      throw UiError(QCoreApplication::translate(
          "RNet", "Complete read confirmation / access release is missing"));
    auto names = QDir(taskDirectory_)
                     .entryList({"node-*-configuration.R-net"}, QDir::Files);
    if (names.isEmpty())
      throw UiError(
          QCoreApplication::translate("RNet", "No configuration received"));
    QString selected = names.first();
    if(names.contains("node-2-configuration.R-net"))selected="node-2-configuration.R-net";
    loadDocument(taskDirectory_ + '/' + selected, true);
    taskLog_->appendPlainText(
        QCoreApplication::translate(
            "RNet", "Reading completed. Raw data, manifest, and CAN log: ") +
        taskDirectory_);
  }
}
void Window::cancelTask() {
  if (process_) {
    cancelRequested_ = true;
    cancel_->setEnabled(false);
    if (process_->processId() > 0)
      ::kill(pid_t(process_->processId()), SIGINT);
    taskLog_->appendPlainText(QCoreApplication::translate(
        "RNet", "Cancellation requested; releasing active access…"));
  } else if (monitorFd_ >= 0)
    stopMonitor();
}
void Window::startMonitor(const QString &interface, int milliseconds) {
  if (busy())
    throw UiError(
        QCoreApplication::translate("RNet", "A CAN task is already running"));
  auto bytes = interface.toLatin1();
  if (bytes.isEmpty() || bytes.size() >= IFNAMSIZ || milliseconds < 1)
    throw UiError(QCoreApplication::translate(
        "RNet", "Invalid CAN interface / duration"));
  int fd = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
  if (fd < 0)
    throw UiError(QCoreApplication::translate("RNet", "Cannot open SocketCAN"));
  ifreq request{};
  std::copy(bytes.begin(), bytes.end(), request.ifr_name);
  sockaddr_can address{};
  address.can_family = AF_CAN;
  if (ioctl(fd, SIOCGIFINDEX, &request) < 0) {
    ::close(fd);
    throw UiError(QCoreApplication::translate("RNet", "Unknown CAN interface"));
  }
  address.can_ifindex = request.ifr_ifindex;
  if (::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
    ::close(fd);
    throw UiError(
        QCoreApplication::translate("RNet", "Cannot bind the CAN interface"));
  }
  monitorFd_ = fd;
  frameCount_ = 0;
  observed_.clear();
  notifier_ = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(notifier_, &QSocketNotifier::activated, this,
          [this] { guarded([this] { receiveFrames(); }); });
  monitorTimer_ = new QTimer(this);
  monitorTimer_->setSingleShot(true);
  connect(monitorTimer_, &QTimer::timeout, this, [this] { stopMonitor(); });
  monitorTimer_->start(milliseconds);
  taskLog_->setPlainText(
      interface +
      QCoreApplication::translate(
          "RNet", ": receiving CAN data; no read or write requests sent."));
  taskPanel_->show();
  cancel_->setText(QCoreApplication::translate("RNet", "Stop capture"));
  cancel_->setEnabled(true);
  updateActions();
  log("CAN_MONITOR_START " + interface + " RX only");
}
void Window::receiveFrames() {
  for (int batch = 0; batch < 512 && monitorFd_ >= 0; ++batch) {
    can_frame frame{};
    auto n = ::read(monitorFd_, &frame, sizeof(frame));
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;
    if (n < 0 && errno == EINTR)
      continue;
    if (n != sizeof(frame)) {
      stopMonitor(false);
      throw UiError(
          QCoreApplication::translate("RNet", "CAN reception failed"));
    }
    if (frame.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG) || frame.can_dlc > 8)
      continue;
    unsigned id = frame.can_id &
                  ((frame.can_id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK);
    auto &sample = observed_[id];
    ++sample.count;
    sample.data =
        QByteArray(reinterpret_cast<const char *>(frame.data), frame.can_dlc);
    ++frameCount_;
  }
}
void Window::stopMonitor(bool show) {
  if (monitorFd_ < 0)
    return;
  notifier_->setEnabled(false);
  notifier_->deleteLater();
  notifier_ = nullptr;
  monitorTimer_->stop();
  monitorTimer_->deleteLater();
  monitorTimer_ = nullptr;
  ::close(monitorFd_);
  monitorFd_ = -1;
  cancel_->setEnabled(false);
  if (show)
    showMonitorCapture();
}
void Window::showMonitorCapture(bool record) {
  QScopedValueRollback<bool> columnGuard(columnLayoutBusy_,true);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_ = View::Monitor;
  valuesView_ = false;
  std::vector<Row> rows;
  for (const auto &item : observed_)
    rows.push_back({hex(item.first),
                    QString::number(item.second.count),
                    QString::fromLatin1(item.second.data.toHex(' ').toUpper()),
                    QString::number(item.second.data.size()),
                    {},
                    QCoreApplication::translate(
                        "RNet", "Raw CAN data; no parameter interpretation. "
                                "This receive path sends no CAN data."),
                    -1});
  model_->replace(std::move(rows),
                  {"CAN-ID", "Frames",
                   QCoreApplication::translate("RNet", "Latest payload"),
                   "DLC"});
  resetGroup(false);
  table_->setColumnHidden(2, false);
  table_->setColumnHidden(3, false);
  table_->setColumnWidth(0,130);
  table_->setColumnWidth(1, 180);
  table_->setColumnWidth(2, 480);
  table_->setColumnWidth(3,80);
  table_->sortByColumn(0,Qt::AscendingOrder);
  resetColumnOrder();
  restoreSavedColumns();
  banner_->setText(QString(QCoreApplication::translate(
                               "RNet", "SocketCAN: received %1 data frames and "
                                       "%2 IDs. No CAN data sent."))
                       .arg(frameCount_)
                       .arg(observed_.size()));
  if (record)
    taskLog_->appendPlainText(banner_->text());
  updateActions();
  if (record)
    log(QString("CAN_MONITOR_DONE frames=%1 ids=%2 TX=0")
            .arg(frameCount_)
            .arg(observed_.size()));
}

void Window::hardwareInfo() {
  QString text = QCoreApplication::translate("RNet", "SocketCAN interfaces:\n");
  for (const auto &name :
       QDir("/sys/class/net").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QFile type("/sys/class/net/" + name + "/type");
    if (type.open(QIODevice::ReadOnly) && type.readAll().trimmed() == "280")
      text += name + "\n";
  }
  text += QCoreApplication::translate("RNet", "\nUSB devices:\n");
  for (const auto &name : QDir("/sys/bus/usb/devices")
                              .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QFile vendor("/sys/bus/usb/devices/" + name + "/idVendor"),
        product("/sys/bus/usb/devices/" + name + "/idProduct");
    if (vendor.open(QIODevice::ReadOnly) && product.open(QIODevice::ReadOnly))
      text += name + ": " + QString::fromLatin1(vendor.readAll().trimmed()) +
              ':' + QString::fromLatin1(product.readAll().trimmed()) + '\n';
  }
  QMessageBox::information(
      this, QCoreApplication::translate("RNet", "Linux CAN devices"), text);
}
void Window::closeEvent(QCloseEvent *event) {
  if(blinkCycle_)blinkCycle_->stop();
  if (process_) {
    cancelTask();
    QMessageBox::information(
        this, QCoreApplication::translate("RNet", "CAN task running"),
        QCoreApplication::translate(
            "RNet", "Cancellation has been requested. Wait for programming "
                    "access to be released, then close the window."));
    event->ignore();
    return;
  }
  try {
    if (!finishChanges()) {
      event->ignore();
      return;
    }
    stopMonitor(false);
    flushColumnSave();
    log("SHUTDOWN clean");
    event->accept();
  } catch (const std::exception &e) {
    QMessageBox::warning(this, "dfreernet", localizedError(e.what()));
    event->ignore();
  }
}
} // namespace rnet::qt
