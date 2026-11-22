#include "i18n.hpp"
#include "window.hpp"
#include "hazard_blink_cycle.hpp"
#include "value_editor.hpp"
#include "app_settings.hpp"
#include "can_heartbeat.hpp"
#include "can_frame_table.hpp"
#include <QtTest>
#include <QtWidgets>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <memory>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef RNET_SOURCE_DIR
#error RNET_SOURCE_DIR required
#endif
namespace rnet::qt {
class WindowTest final : public QObject {
  Q_OBJECT
  QTemporaryDir temp_;
  QString root_ = QString::fromUtf8(RNET_SOURCE_DIR);
  QString fixture_ = root_ + "/tests/fixtures/user-documents/CS16110708.R-net";
  std::unique_ptr<Window> make() {
    auto defaults=AppSettings::defaults();
    defaults.canInterface="rnetOffline";defaults.keepAwake=false;defaults.showAllParameters=false;
    defaults.profileTableView=false;defaults.moduleGroupView=false; // Never bind a physical interface in offline UI tests.
    defaults.workingDirectory=temp_.path()+"/documents";defaults.backupDirectory=temp_.path()+"/backups";
    defaults.exportDirectory=temp_.path()+"/exports";defaults.logDirectory=temp_.path()+"/logs";defaults.save();
    languages().setLanguage("en_US");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    auto w = std::make_unique<Window>(temp_.path());
    w->setAccess(AccessLevel::OEM);
    w->show();
    QTest::qWait(30);
    return w;
  }
  static int firstSpeed(Window &w) {
    for (std::size_t i = 0; i < w.model()->rows.size(); ++i) {
      auto &row = w.model()->rows[i];
      if (row.id == "312" && row.location == "Profile 1 / 1")
        return row.valueIndex;
    }
    return -1;
  }
private slots:
  void canFrameTableAverage() {
    CanFrameTable table(nullptr, "averageTest", QFont());
    table.show();
    QCOMPARE(table.columnCount(), 4);
    QCOMPARE(table.horizontalHeaderItem(2)->text(), QString("avg [Hz]"));
    table.observe("123", 2, "01 02");
    table.observe("123", 2, "03 04");
    table.refresh();
    QCOMPARE(table.item(0, 0)->text(), QString("       2"));
    QCOMPARE(table.item(0, 3)->text(), QString("[2] 03 04"));
    const double initial = table.item(0, 2)->text().toDouble();
    QVERIFY(initial > 0.0 && initial <= 2.0);
    table.setAverageUnit(CanFrameTable::AverageUnit::Milliseconds);
    QCOMPARE(table.horizontalHeaderItem(2)->text(), QString("avg [ms]"));
    QVERIFY(qAbs(table.item(0, 2)->text().toDouble() - 1000.0 / initial) < 10.0);
    table.setAverageUnit(CanFrameTable::AverageUnit::Hertz);
    QTRY_VERIFY_WITH_TIMEOUT(table.item(0, 2)->text().toDouble() < initial, 3000);
    QVERIFY(table.item(0, 2)->text().toDouble() > 0.0);
    table.resetCounts();
    QCOMPARE(table.rowCount(), 0);
    table.observe("124", 1, "AA");
    table.refresh();
    QCOMPARE(table.item(0, 0)->text(), QString("       1"));
    QVERIFY(table.item(0, 2)->text().toDouble() > 0.0);
  }
  void canFrameTableSort() {
    QSettings().remove("ui/columns/v1/sortTest");
    QSettings().remove("ui/canSort/v1/sortTest");
    {
      CanFrameTable table(nullptr, "sortTest", QFont());
      table.resize(720, 300);
      table.show();
      for (int i = 0; i < 2; ++i) table.observe("001", 1, "01");
      for (int i = 0; i < 10; ++i) table.observe("002", 1, "02");
      table.observe("003", 1, "03");
      table.refresh();
      QCOMPARE(table.horizontalHeader()->sortIndicatorSection(), 1);
      QCOMPARE(table.item(0, 1)->text(), QString("001"));
      table.sortItems(0, Qt::AscendingOrder);
      QCOMPARE(table.item(0, 0)->text(), QString("       1"));
      QCOMPARE(table.item(2, 0)->text(), QString("      10"));
      auto *header = table.horizontalHeader();
      QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                        QPoint(header->sectionViewportPosition(0) + 20, header->height() / 2));
      QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
      QCOMPARE(table.item(0, 0)->text(), QString("      10"));
      table.observe("001", 1, "AA");
      table.refresh();
      QCOMPARE(table.item(0, 1)->text(), QString("002"));
      QCOMPARE(table.item(1, 1)->text(), QString("001"));
      QCOMPARE(table.item(1, 0)->text(), QString("       3"));
      QCOMPARE(table.item(1, 3)->text(), QString("[1] AA"));
      QVERIFY(table.item(0, 2)->text().toDouble() > table.item(1, 2)->text().toDouble());
      table.sortItems(2, Qt::AscendingOrder);
      QCOMPARE(table.item(0, 1)->text(), QString("003"));
      QCOMPARE(table.item(2, 1)->text(), QString("002"));
      table.setAverageUnit(CanFrameTable::AverageUnit::Milliseconds);
      QCOMPARE(table.item(0, 1)->text(), QString("002"));
      QCOMPARE(table.item(2, 1)->text(), QString("003"));
      table.observe("00F", 1, "0F");
      table.observe("00000010", 1, "10");
      table.refresh();
      table.sortItems(1, Qt::AscendingOrder);
      QCOMPARE(table.item(3, 1)->text(), QString("00F"));
      QCOMPARE(table.item(4, 1)->text(), QString("00000010"));
    }
    QSettings().remove("ui/columns/v1/sortTest");
    QSettings().remove("ui/canSort/v1/sortTest");
  }
  void canViewPreferencesRestore() {
    QSettings settings;
    settings.remove("ui/columns/v1/layoutTestCount");
    settings.remove("ui/columns/v1/layoutTestSystem");
    settings.remove("ui/columns/v1/liveCanCount");
    settings.remove("ui/columns/v1/systemTestFrames");
    settings.remove("ui/canSort/v1/layoutTestCount");
    settings.remove("ui/canSort/v1/layoutTestSystem");
    settings.remove("ui/canSort/v1/liveCanCount");
    settings.remove("ui/canSort/v1/systemTestFrames");
    settings.remove("ui/canDumpMode");
    settings.remove("ui/canDumpUnit");
    settings.remove("ui/systemTestMode");
    settings.remove("ui/systemTestUnit");
    settings.remove("ui/systemTestGeometry");
    settings.remove("ui/contentSplitterState");
    QList<int> savedContentSizes;
    {
      CanFrameTable count(nullptr, "layoutTestCount", QFont());
      CanFrameTable system(nullptr, "layoutTestSystem", QFont());
      count.horizontalHeader()->moveSection(2, 0);
      count.setColumnWidth(1, 147);
      count.sortItems(0, Qt::DescendingOrder);
      system.horizontalHeader()->moveSection(3, 1);
      system.setColumnWidth(0, 164);
      system.sortItems(2, Qt::DescendingOrder);
    }
    {
      CanFrameTable count(nullptr, "layoutTestCount", QFont());
      CanFrameTable system(nullptr, "layoutTestSystem", QFont());
      QCOMPARE(count.horizontalHeader()->visualIndex(2), 0);
      QCOMPARE(count.columnWidth(1), 147);
      QCOMPARE(count.horizontalHeader()->sortIndicatorSection(), 0);
      QCOMPARE(count.horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
      QCOMPARE(system.horizontalHeader()->visualIndex(3), 1);
      QCOMPARE(system.columnWidth(0), 164);
      QCOMPARE(system.horizontalHeader()->sortIndicatorSection(), 2);
      QCOMPARE(system.horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
      QCOMPARE(count.horizontalHeader()->visualIndex(3), 3);
      QCOMPARE(system.columnWidth(1), 115);
    }
    {
      auto window = make();
      auto *countMode = window->findChild<QToolButton *>("liveCanViewCount");
      auto *msUnit = window->findChild<QToolButton *>("liveCanViewMs");
      QVERIFY(countMode && msUnit);
      countMode->click();
      msUnit->click();
      window->contentSplitter_->setSizes({420, 780});
      savedContentSizes = window->contentSplitter_->sizes();
      auto *count = dynamic_cast<CanFrameTable *>(window->findChild<QTableWidget *>("liveCanCount"));
      QVERIFY(count);
      count->horizontalHeader()->moveSection(2, 0);
      count->setColumnWidth(1, 148);
      count->sortItems(0, Qt::DescendingOrder);
      window->showSystemTest(33395);
      auto *dialog = window->systemTestDialog_.data();
      QVERIFY(dialog);
      auto *system = dynamic_cast<CanFrameTable *>(dialog->findChild<QTableWidget *>("systemTestFrames"));
      QVERIFY(system);
      auto *systemFollow = dialog->findChild<QToolButton *>("systemTestViewFollow");
      auto *systemMs = dialog->findChild<QToolButton *>("systemTestViewMs");
      QVERIFY(systemFollow && systemMs);
      systemFollow->click();
      systemMs->click();
      system->horizontalHeader()->moveSection(3, 1);
      system->setColumnWidth(0, 165);
      system->sortItems(2, Qt::DescendingOrder);
      dialog->resize(750, 440);
      dialog->close();
      QTRY_VERIFY(window->systemTestDialog_.isNull());
      QVERIFY(window->close());
    }
    {
      auto window = make();
      QVERIFY(window->findChild<QToolButton *>("liveCanViewCount")->isChecked());
      QVERIFY(window->findChild<QToolButton *>("liveCanViewMs")->isChecked());
      QCOMPARE(window->contentSplitter_->sizes(), savedContentSizes);
      auto *count = dynamic_cast<CanFrameTable *>(window->findChild<QTableWidget *>("liveCanCount"));
      QVERIFY(count);
      QCOMPARE(count->horizontalHeader()->visualIndex(2), 0);
      QCOMPARE(count->columnWidth(1), 148);
      QCOMPARE(count->horizontalHeader()->sortIndicatorSection(), 0);
      QCOMPARE(count->horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
      window->showSystemTest(33395);
      QVERIFY(window->systemTestDialog_->findChild<QToolButton *>("systemTestViewFollow")->isChecked());
      QVERIFY(window->systemTestDialog_->findChild<QToolButton *>("systemTestViewMs")->isChecked());
      auto *system = dynamic_cast<CanFrameTable *>(window->systemTestDialog_->findChild<QTableWidget *>("systemTestFrames"));
      QVERIFY(system);
      QCOMPARE(system->horizontalHeader()->visualIndex(3), 1);
      QCOMPARE(system->columnWidth(0), 165);
      QCOMPARE(system->horizontalHeader()->sortIndicatorSection(), 2);
      QCOMPARE(system->horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
      QCOMPARE(window->systemTestDialog_->size(), QSize(750, 440));
    }
    settings.remove("ui/columns/v1/layoutTestCount");
    settings.remove("ui/columns/v1/layoutTestSystem");
    settings.remove("ui/columns/v1/liveCanCount");
    settings.remove("ui/columns/v1/systemTestFrames");
    settings.remove("ui/canSort/v1/layoutTestCount");
    settings.remove("ui/canSort/v1/layoutTestSystem");
    settings.remove("ui/canSort/v1/liveCanCount");
    settings.remove("ui/canSort/v1/systemTestFrames");
    settings.remove("ui/canDumpMode");
    settings.remove("ui/canDumpUnit");
    settings.remove("ui/systemTestMode");
    settings.remove("ui/systemTestUnit");
    settings.remove("ui/systemTestGeometry");
    settings.remove("ui/contentSplitterState");
  }
  void profileTableSwitchAndEdit() {
    auto w=make();w->openDocument(fixture_);
    const int valueIndex=firstSpeed(*w);
    QVERIFY(valueIndex>=0);
    QString original;
    for(const auto &row:w->model()->rows)
      if(row.valueIndex==valueIndex)original=row.value;
    QVERIFY(!original.isEmpty());
    auto *profileAction=w->findChild<QAction*>("profileTableView");
    auto *listAction=w->findChild<QAction*>("hierarchicalListView");
    QVERIFY(profileAction&&listAction);
    profileAction->trigger();
    QVERIFY(profileAction->isChecked());
    auto *tree=w->findChild<QTreeWidget*>("groups");
    QCOMPARE(tree->topLevelItemCount(),9);
    QCOMPARE(tree->topLevelItem(0)->text(0),QString("Common"));
    QCOMPARE(tree->topLevelItem(8)->text(0),QString("Profile 8"));
    tree->setCurrentItem(tree->topLevelItem(1));
    QCOMPARE(w->filter()->group,QString("profile1"));
    int targetRow=-1;
    for(int row=0;row<w->model()->rowCount();++row)
      if(w->model()->rows[std::size_t(row)].valueIndex==valueIndex){
        targetRow=row;break;
      }
    QVERIFY(targetRow>=0);
    auto cell=w->model()->index(targetRow,2);
    QCOMPARE(cell.data().toString(),original);
    QVERIFY(cell.flags()&Qt::ItemIsEditable);
    auto *table=w->findChild<QTableView*>("parameters");
    table->setCurrentIndex(w->filter()->mapFromSource(cell));
    QVERIFY(w->edit_->isEnabled());
    w->applyEdit(valueIndex,"86");
    bool changed=false;
    for(int row=0;row<w->model()->rowCount();++row){
        auto index=w->model()->index(row,2);
        if(w->model()->rows[std::size_t(row)].valueIndex!=valueIndex)continue;
        QCOMPARE(index.data().toString(),QString("86"));
        QCOMPARE(index.data(Qt::ForegroundRole).value<QColor>(),QColor(Qt::red));
        changed=true;
    }
    QVERIFY(changed);
    listAction->trigger();
    QVERIFY(listAction->isChecked());
    QCOMPARE(w->model()->rowCount(),w->valueCount());
  }
  void initTestCase() {
    QCoreApplication::setOrganizationName("RNetTests");
    QCoreApplication::setApplicationName("RNetQtTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       temp_.path() + "/settings");
    auto settings=AppSettings::defaults();settings.canInterface="rnetOffline";settings.keepAwake=false;settings.save();
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
  }
  void languageDefaultAndPreference() {
    QCOMPARE(languages().language(), QString("en_US"));
    QCOMPARE(QLocale().language(), QLocale::English);
    QCOMPARE(QLocale().territory(), QLocale::UnitedStates);
    QCOMPARE(languages().savedLanguage(), QString("en_US"));
    languages().setLanguage("de_DE", true);
    QCOMPARE(languages().savedLanguage(), QString("de_DE"));
    languages().setLanguage("en_US", true);
    QCOMPARE(languages().savedLanguage(), QString("en_US"));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                             languages().setLanguage("fr_FR"));
  }
  void germanStartupAndStandardButtons() {
    languages().setLanguage("de_DE");
    auto w = std::make_unique<Window>(temp_.path());
    w->show();
    QTest::qWait(20);
    QCOMPARE(w->findChild<QAction *>("command57601")->text(),
             QString("Datei lesen…"));
    QCOMPARE(w->findChild<QMenu *>("languageMenu")->title(),
             QString("Sprache"));
    QCOMPARE(w->findChild<QAction *>("role3")->text(), QString("Werk"));
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    QCOMPARE(
        QString(buttons.button(QDialogButtonBox::Save)->text()).remove('&'),
        QString("Speichern"));
    QCOMPARE(
        QString(buttons.button(QDialogButtonBox::Cancel)->text()).remove('&'),
        QString("Abbrechen"));
    w->openDocument(fixture_);
    auto bytes = w->document()->write();
    w->showRepository();
    auto count = w->model()->rowCount();
    languages().setLanguage("en_US");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QTRY_COMPARE(w->model()->headings[0], QString("File ID"));
    QVERIFY(w->findChild<QLabel *>("provenanceBanner")
                ->text()
                .contains("Stored raw values"));
    QCOMPARE(w->model()->rowCount(), count);
    w->showResources();
    languages().setLanguage("de_DE");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QTRY_COMPARE(w->model()->headings[0], QString("Ressourcen-ID"));
    QVERIFY(w->findChild<QLabel *>("provenanceBanner")
                ->text()
                .contains("Eingebettete Textressourcen"));
    QVERIFY(w->document()->write() == bytes);
    w.reset();
    languages().setLanguage("en_US");
  }
  void liveLanguageSwitchPreservesDocumentAndUiState() {
    auto w = make();
    w->openDocument(fixture_);
    auto original = w->document()->write();
    int index = firstSpeed(*w);
    QVERIFY(index >= 0);
    w->applyEdit(index, "86");
    auto edited = w->document()->write();
    QVERIFY(edited != original);
    auto *search = w->findChild<QLineEdit *>("search");
    search->setText("Maximum Forward Speed");
    auto *table = w->findChild<QTableView *>("parameters");
    int source = -1;
    for (int i = 0; i < w->model()->rowCount(); ++i)
      if (w->model()->rows[std::size_t(i)].valueIndex == index)
        source = i;
    QVERIFY(source >= 0);
    auto *tree = w->findChild<QTreeWidget *>("groups");
    auto selectedGroup = w->model()->rows[std::size_t(source)].group;
    for (QTreeWidgetItemIterator item(tree); *item; ++item)
      if ((*item)->data(0, Qt::UserRole).toString() == selectedGroup) {
        tree->setCurrentItem(*item);
        for (auto *p = (*item)->parent(); p; p = p->parent())
          p->setExpanded(true);
        break;
      }
    table->setCurrentIndex(
        w->filter()->mapFromSource(w->model()->index(source, 0)));
    auto *menu = w->findChild<QMenu *>("languageMenu");
    QVERIFY(menu);
    auto change = [&](const QString &code) {
      QTest::mouseClick(
          w->menuBar(), Qt::LeftButton, Qt::NoModifier,
          w->menuBar()->actionGeometry(menu->menuAction()).center());
      QTest::qWait(10);
      auto *action = w->findChild<QAction *>("language_" + code);
      QVERIFY(action);
      QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                        menu->actionGeometry(action).center());
    };
    change("de_DE");
    QTRY_COMPARE(w->model()->headings[2], QString("Gespeicherter Rohwert"));
    QCOMPARE(menu->title(), QString("Sprache"));
    QCOMPARE(w->findChild<QAction *>("command57601")->text(),
             QString("Datei lesen…"));
    QVERIFY(w->findChild<QAction *>("command32803")->text().contains("lesen"));
    QCOMPARE(w->findChild<QTreeWidget *>("groups")->topLevelItem(0)->text(0),
             QString("Gelesene Parameter"));
    QCOMPARE(w->filter()->group, selectedGroup);
    QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toString(),
             selectedGroup);
    for (auto *p = tree->currentItem()->parent(); p; p = p->parent())
      QVERIFY(p->isExpanded());
    QCOMPARE(search->text(), QString("Maximum Forward Speed"));
    QCOMPARE(w->filter()->rowCount(), 8);
    QCOMPARE(w->undo_.size(), std::size_t(1));
    QVERIFY(w->document()->write() == edited);
    QVERIFY(w->isWindowModified());
    QCOMPARE(w->filter()->mapToSource(table->currentIndex()).row(), source);
    QCOMPARE(localizedError("Enter an integer"),
             QString("Eine ganze Zahl eingeben"));
    QVERIFY(localizedError("Value must be between 1 and 100")
                .contains("zwischen 1 und 100"));
    change("en_US");
    QTRY_COMPARE(w->model()->headings[2], QString("Stored raw value"));
    QCOMPARE(menu->title(), QString("Language"));
    QCOMPARE(w->findChild<QAction *>("command57601")->text(),
             QString("Read file…"));
    QVERIFY(w->document()->write() == edited);
    w->undo();
    QVERIFY(w->document()->write() == original);
    QVERIFY(read_file(fixture_.toStdString()) == original);
  }
  void unreadCatalogRowsAreFilteredGrayAndNeverEditable(){
    auto w=make();w->settings_.showAllParameters=true;w->setAccess(AccessLevel::Dealer);w->showValues();
    QVERIFY(w->model()->rowCount()>2000);QVERIFY(w->filter()->rowCount()>0);QVERIFY(!w->document());
    for(int i=0;i<w->filter()->rowCount();++i){auto index=w->filter()->index(i,2);auto source=w->filter()->mapToSource(index);const auto &row=w->model()->rows[source.row()];QVERIFY(row.unread);QVERIFY(row.requiredAccess<=1);QCOMPARE(row.valueIndex,-1);QCOMPARE(index.data().toString(),QString("Not read"));QVERIFY(!(index.flags()&Qt::ItemIsEditable));QCOMPARE(index.data(Qt::ForegroundRole).value<QColor>(),QColor(Qt::gray));}
    w->openDocument(fixture_);QVERIFY(w->model()->rowCount()>w->valueCount());auto bytes=w->document()->write();int unread=-1;
    for(int i=0;i<w->model()->rowCount();++i)if(w->model()->rows[i].unread){unread=i;break;}QVERIFY(unread>=0);QVERIFY(!(w->model()->flags(w->model()->index(unread,2))&Qt::ItemIsEditable));
    auto plan=controller_write_plan(*w->document(),*w->document(),w->catalog_,AccessLevel::Dealer);QVERIFY(plan.empty());
    w->settings_.showAllParameters=false;w->showValues();QCOMPARE(w->model()->rowCount(),w->valueCount());QVERIFY(w->document()->write()==bytes);
    w->settings_.showAllParameters=true;w->showValues();w->setAccess(AccessLevel::Therapist);for(int i=0;i<w->filter()->rowCount();++i)QVERIFY(w->filter()->index(i,5).data().toString().startsWith("0"));
  }
  void coloredDongleStatusTracksRolesAndLanguage(){
    auto w=make();auto *label=w->findChild<QLabel*>("dongleStatus");QVERIFY(label);const QString colors[]{"#ffeb3b","#4caf50","#1976d2","#d32f2f"};
    for(int i=0;i<4;++i){w->setAccess(AccessLevel(i));QVERIFY(label->text().contains(QString::number(i)));QVERIFY(label->styleSheet().contains(colors[i]));}
    w->setAccess(AccessLevel::Dealer);languages().setLanguage("de_DE");QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);QVERIFY(label->text().contains("Händler"));QVERIFY(label->styleSheet().contains(colors[1]));
    w->statusBar()->showMessage("Temporary status");QVERIFY(label->isVisible());languages().setLanguage("en_US");
  }
  void elevatedMenuRequiresWarningAndDoesNotPersist(){
    auto w=make();w->setAccess(AccessLevel::Dealer);
    if(!qEnvironmentVariable("DFREERNET_DOC_SCREENSHOT").isEmpty()){
      languages().setLanguage(qEnvironmentVariable("DFREERNET_DOC_LANGUAGE","de_DE"),true);
      QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);
    }
    QTimer::singleShot(0,[]{
      auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);
      if(const auto path=qEnvironmentVariable("DFREERNET_DOC_SCREENSHOT");!path.isEmpty())
        QVERIFY(box->grab().save(path+"-dongle.png"));
      box->button(QMessageBox::Cancel)->click();
    });w->findChild<QAction*>("role3")->trigger();QCOMPARE(w->access_,AccessLevel::Dealer);QVERIFY(w->findChild<QAction*>("role1")->isChecked());
    if(!qEnvironmentVariable("DFREERNET_DOC_SCREENSHOT").isEmpty()){
      languages().setLanguage("en_US",true);
      QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);
    }
    QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Yes)->click();});w->findChild<QAction*>("role2")->trigger();QCOMPARE(w->access_,AccessLevel::OEM);QCOMPARE(AppSettings::load().accessLevel,1);
    w->close();
  }
  void catalogAndSearch() {
    auto w = make();
    QCOMPARE(w->model()->rowCount(), 0);
    QCOMPARE(
        w->findChild<QTreeWidget *>("groups")->topLevelItem(0)->childCount(),
        0);
    w->openDocument(fixture_);
    QCOMPARE(w->model()->rowCount(), w->valueCount());
    auto *search = w->findChild<QLineEdit *>("search");
    QTest::mouseClick(search, Qt::LeftButton);
    QTest::keyClicks(search, "Maximum Forward Speed");
    QVERIFY(w->filter()->rowCount() > 0);
    QVERIFY(w->filter()->rowCount() < 10);
    search->clear();
    auto *tree = w->findChild<QTreeWidget *>("groups");
    auto matches =
        tree->findItems("Controls", Qt::MatchExactly | Qt::MatchRecursive);
    QVERIFY(!matches.isEmpty());
    tree->setCurrentItem(matches.first());
    QVERIFY(w->filter()->rowCount() > 0);
    QVERIFY(w->filter()->rowCount() < w->valueCount());
  }
  void readWriteFileThroughToolbar() {
    auto w = make();
    auto *toolbar = w->findChild<QToolBar *>("readFileToolbar");
    QVERIFY(toolbar);
    auto *open = w->findChild<QAction *>("command57601");
    auto *write = w->findChild<QAction *>("command57604");
    QVERIFY(open && write);
    QVERIFY(!write->isEnabled());
    QTimer::singleShot(100, w.get(), [this] {
      for (auto *widget : QApplication::topLevelWidgets())
        if (auto *dialog = qobject_cast<QFileDialog *>(widget);
            dialog && dialog->isVisible()) {
          dialog->setDirectory(QFileInfo(fixture_).absolutePath());
          QTimer::singleShot(200, dialog, [this, dialog] {
            auto *name = dialog->findChild<QLineEdit *>("fileNameEdit");
            if (name)
              name->setText(fixture_);
            else
              dialog->selectFile(fixture_);
            qInfo() << "Open dialog selection" << dialog->selectedFiles();
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
          });
          QTimer::singleShot(3000, dialog, &QDialog::reject);
        }
    });
    QTest::mouseClick(toolbar->widgetForAction(open), Qt::LeftButton);
    QVERIFY(w->document());
    QVERIFY(write->isEnabled());
    auto bytes = w->document()->write();
    auto destination = temp_.path() + "/toolbar-copy.R-net";
    QTimer::singleShot(100, w.get(), [destination] {
      for (auto *widget : QApplication::topLevelWidgets())
        if (auto *dialog = qobject_cast<QFileDialog *>(widget);
            dialog && dialog->isVisible()) {
          dialog->setDirectory(QFileInfo(destination).absolutePath());
          QTimer::singleShot(200, dialog, [destination, dialog] {
            auto *name = dialog->findChild<QLineEdit *>("fileNameEdit");
            if (name)
              name->setText(destination);
            else
              dialog->selectFile(destination);
            qInfo() << "Save dialog selection" << dialog->selectedFiles();
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
          });
          QTimer::singleShot(3000, dialog, &QDialog::reject);
        }
    });
    QTest::mouseClick(toolbar->widgetForAction(write), Qt::LeftButton);
    QVERIFY(QFileInfo::exists(destination));
    QVERIFY(read_file(destination.toStdString()) == bytes);
    QVERIFY(read_file(fixture_.toStdString()) == bytes);
    w->closeDocument();
    QCOMPARE(w->model()->rowCount(), 0);
    QCOMPARE(
        w->findChild<QTreeWidget *>("groups")->topLevelItem(0)->childCount(),
        0);
  }
  void groupsContainOnlyReadValues() {
    auto w = make();
    w->openDocument(fixture_);
    QSet<QString> expected;
    for (const auto &row : w->model()->rows) {
      if (row.requiredAccess > int(w->access()))
        continue;
      QString full;
      for (const auto &part : row.group.split('~', Qt::SkipEmptyParts)) {
        if (!full.isEmpty())
          full += '~';
        full += part;
        expected.insert(full);
      }
    }
    QSet<QString> actual;
    auto *tree = w->findChild<QTreeWidget *>("groups");
    QTreeWidgetItemIterator it(tree);
    while (*it) {
      auto group = (*it)->data(0, Qt::UserRole).toString();
      if (!group.isEmpty())
        actual.insert(group);
      ++it;
    }
    QCOMPARE(actual, expected);
    for (const auto &row : w->model()->rows)
      QVERIFY(row.valueIndex >= 0);
  }
  void blinkCannotArmWithoutCurrentHeartbeat() {
    auto w = make();
    QVERIFY(!w->heartbeatReceiving());
    auto *action = w->findChild<QAction*>("hazardBlinkAction");
    QVERIFY(action);
    QVERIFY_THROWS_EXCEPTION(UiError,w->requestBlinkCycle());
    QVERIFY(!action->isChecked());
    QVERIFY(!w->blinkCycle_->active());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
  }
  void rnetClockStatusShowsFreshTimeAndStaleState(){
    auto w=make();
    auto *label=w->findChild<QLabel*>("rnetClockStatus");QVERIFY(label);
    QVERIFY(label->text().contains("unavailable"));
    w->rnetClock_=decodeRnetClockFrame(CAN_EFF_FLAG|0x1c2c0000U,QByteArray::fromHex("281604ef0b1a"));
    w->rnetClockAge_.restart();w->updateRnetClockStatus();
    QVERIFY(label->text().contains("04:22:40"));
    QVERIFY(label->toolTip().contains("2026-11-15"));
    languages().setLanguage("de_DE",true);
    QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);
    QTRY_VERIFY(label->text().contains("R-Net-Zeit"));
    QVERIFY(w->grab().save(root_+"/analysis/rnet-clock-status-de.png"));
    QVERIFY(!w->pendingRnetClock_.isValid());
    w->rnetClockAge_.invalidate();w->updateRnetClockStatus();
    QVERIFY(label->text().contains("nicht verfügbar"));
    languages().setLanguage("en_US",true);
  }
  void rnetClockStatusColorsTrackHostDifference(){
    auto w=make();
    auto *label=w->findChild<QLabel*>("rnetClockStatus");QVERIFY(label);
    const auto host=QDateTime::currentDateTime();
    auto check=[&](int offsetSeconds,const char *color){
      const auto rnet=host.addSecs(offsetSeconds);
      w->rnetClock_={rnet.date(),rnet.time()};
      w->rnetClockAge_.restart();w->updateRnetClockStatus();
      QVERIFY(label->styleSheet().contains(QLatin1String(color)));
    };
    check(-59,"#2e9b44");
    check(61,"#e67e00");
    check(-899,"#e67e00");
    check(901,"#d32f2f");
    QVERIFY(w->grab().save(root_+"/analysis/rnet-clock-offset-red.png"));
    w->rnetClockAge_.invalidate();w->updateRnetClockStatus();
    QVERIFY(label->styleSheet().isEmpty());
  }
  void rnetDateStatusMatchesHostDate(){
    auto w=make();
    auto *date=w->findChild<QLabel*>("rnetDateStatus");QVERIFY(date);
    QVERIFY(!date->isVisible());
    const auto host=QDateTime::currentDateTime();
    w->rnetClock_={host.date(),host.time()};
    w->rnetClockAge_.restart();w->updateRnetClockStatus();
    QVERIFY(date->isVisible());
    QVERIFY(date->text().contains(host.date().toString(Qt::ISODate)));
    QVERIFY(date->styleSheet().contains("#2e9b44"));
    w->rnetClock_={host.date().addDays(1),host.time()};
    w->rnetClockAge_.restart();w->updateRnetClockStatus();
    QVERIFY(date->styleSheet().contains("#d32f2f"));
    const auto screenshotLanguage=qEnvironmentVariable("DFREERNET_DOC_LANGUAGE");
    if(screenshotLanguage=="en_US"||screenshotLanguage=="de_DE"){
      languages().setLanguage(screenshotLanguage,true);
      QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);
    }
    const auto suffix=screenshotLanguage=="de_DE"?QStringLiteral("-de"):QStringLiteral("-en");
    QVERIFY(w->grab().save(root_+"/analysis/rnet-date-offset-red"+suffix+".png"));
    w->rnetClockAge_.invalidate();w->updateRnetClockStatus();
    QVERIFY(!date->isVisible());
    languages().setLanguage("en_US",true);
  }
  void rnetClockSetOnPrivateVcan(){
    const auto interface=qEnvironmentVariable("RNET_CLOCK_VCAN");
    if(interface!="rnetclock0")QSKIP("Only run in isolated clock test namespace");
    auto w=make();w->settings_.canInterface=interface;w->applyPreferences();
    QTRY_VERIFY_WITH_TIMEOUT(w->liveDumpFd_>=0,2000);
    int fd=::socket(PF_CAN,SOCK_RAW|SOCK_NONBLOCK,CAN_RAW);QVERIFY(fd>=0);
    ifreq request{};qstrncpy(request.ifr_name,"rnetclock0",IFNAMSIZ);
    QVERIFY(::ioctl(fd,SIOCGIFINDEX,&request)==0);
    sockaddr_can address{};address.can_family=AF_CAN;address.can_ifindex=request.ifr_ifindex;
    QVERIFY(::bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
    can_frame frame{};frame.can_id=CAN_EFF_FLAG|rnetClockBroadcastId;frame.can_dlc=6;
    const auto before=QByteArray::fromHex("281604ef0b1a");
    std::copy(before.begin(),before.end(),frame.data);
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    auto *label=w->findChild<QLabel*>("rnetClockStatus");QVERIFY(label);
    QTRY_VERIFY_WITH_TIMEOUT(label->text().contains("04:22:40"),2000);
    QTest::mouseClick(label,Qt::LeftButton);
    QVERIFY(w->pendingRnetClock_.isValid());
    can_frame sent{};bool found=false;QElapsedTimer deadline;deadline.start();
    while(deadline.elapsed()<2000&&!found){
      can_frame candidate{};
      const auto size=::read(fd,&candidate,sizeof(candidate));
      if(size==sizeof(candidate)&&candidate.can_id==(CAN_EFF_FLAG|rnetClockSetId)){
        sent=candidate;found=true;
      }else QTest::qWait(10);
    }
    QVERIFY(found);QCOMPARE(sent.can_dlc,6);
    const auto payload=QByteArray(reinterpret_cast<const char*>(sent.data),6);
    const auto decoded=decodeRnetClockFrame(CAN_EFF_FLAG|rnetClockBroadcastId,payload);
    QVERIFY(decoded.isValid());QCOMPARE(decoded.date,QDate::currentDate());
    QVERIFY(qAbs(decoded.time.secsTo(QTime::currentTime()))<=2);
    sent.can_id=CAN_EFF_FLAG|rnetClockBroadcastId;
    QCOMPARE(::write(fd,&sent,sizeof(sent)),ssize_t(sizeof(sent)));
    QTRY_VERIFY_WITH_TIMEOUT(!w->pendingRnetClock_.isValid(),2000);
    QVERIFY(w->statusBar()->currentMessage().contains("confirmed"));
    ::close(fd);
  }
  void persistentSettingsAndPassiveStatusWidget(){
    auto w=make();w->openDocument(fixture_);const auto bytes=w->document()->write();
    auto settings=AppSettings::load();settings.canInterface="rnetSaved";settings.backupDirectory=temp_.path()+"/saved-backups";
    settings.heartbeatEnabled=true;settings.itemIconSize=22;settings.toolbarIconsOnly=true;settings.save();
    auto restored=std::make_unique<Window>(temp_.path()+"/second-log");restored->show();
    QCOMPARE(restored->selectInterface(),QString("rnetSaved"));
    auto backup=settings.newBackupDirectory("rnet-config");
    QVERIFY(backup.startsWith(settings.backupDirectory+"/rnet-config-"));QVERIFY(!QFileInfo::exists(backup));
    QVERIFY(restored->heartbeat_);QVERIFY(!restored->heartbeat_->receiving());
    QVERIFY(restored->heartbeat_->toolTip().contains("rnetSaved"));
    QCOMPARE(restored->findChild<QTreeWidget*>("groups")->iconSize(),QSize(22,22));
    auto *toolbar = restored->findChild<QToolBar*>("readFileToolbar");
    QVERIFY(toolbar);
    QCOMPARE(toolbar->toolButtonStyle(), Qt::ToolButtonIconOnly);
    for (auto *item : toolbar->actions()) {
      if (!item->isSeparator()) {
        QVERIFY(!item->icon().isNull());
        QVERIFY(!item->toolTip().isEmpty());
      }
    }
    QTimer::singleShot(0, restored.get(), [] {
      auto *dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      dialog->findChild<QCheckBox*>("settingsToolbarIconsOnly")->setChecked(false);
      dialog->findChild<QDialogButtonBox*>("settingsButtons")->button(QDialogButtonBox::Apply)->click();
      dialog->reject();
    });
    restored->openSettings();
    QCOMPARE(toolbar->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    QVERIFY(!AppSettings::load().toolbarIconsOnly);
    auto *action=w->findChild<QAction*>("command40040");QVERIFY(action&&action->isEnabled());
    QTimer::singleShot(20,w.get(),[]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
    action->trigger();QCOMPARE(w->document()->write(),bytes);QVERIFY(w->process_==nullptr);
  }
  void originalControllerReadMenuEnabledAndRoutesToReadDialog() {
    auto w = make();
    auto *read = w->findChild<QAction *>("command32803");
    QVERIFY(read);
    QVERIFY(read->isEnabled());
    QVERIFY(read->text().contains("Read from Controller"));
    bool dialogSeen = false;
    QTimer::singleShot(50, w.get(), [&] {
      if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
        dialogSeen = dialog->windowTitle()=="Request and read configuration only" && !qobject_cast<QInputDialog*>(dialog);
        QVERIFY(dialog->findChildren<QCheckBox*>().isEmpty());
        auto *buttons=dialog->findChild<QDialogButtonBox*>();
        QVERIFY(buttons&&buttons->button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(dialog->grab().save(root_+"/analysis/controller-read-dialog.png"));
        dialog->reject();
      }
    });
    read->trigger();
    QVERIFY(dialogSeen);
    QVERIFY(w->process_ == nullptr);
    if(QFileInfo::exists(fixture_)){
      w->openDocument(fixture_);
      QVERIFY(read->isEnabled());
    }
  }
  void clockMenuOpensSettingsWithoutSending() {
    auto w=make();
    auto *action=w->findChild<QAction*>("command33051");
    QVERIFY(action&&action->isEnabled());
    bool dialogSeen=false;
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      dialogSeen=dialog->objectName()=="clockSettingsDialog";
      auto *host=dialog->findChild<QTimeEdit*>("clockComputerTime");
      auto *time=dialog->findChild<QTimeEdit*>("clockRnetTime");
      auto *hostDate=dialog->findChild<QDateEdit*>("clockComputerDate");
      auto *date=dialog->findChild<QDateEdit*>("clockRnetDate");
      QVERIFY(host&&hostDate&&date&&time);
      QVERIFY(host->isReadOnly());
      QVERIFY(hostDate->isReadOnly());
      QCOMPARE(host->displayFormat(),QString("HH:mm:ss"));
      QCOMPARE(time->displayFormat(),QString("HH:mm:ss"));
      auto *grid=dialog->findChild<QGridLayout*>("clockGrid");
      QVERIFY(grid);
      QCOMPARE(grid->rowCount(),4);
      QCOMPARE(grid->columnCount(),3);
      int row=0,column=0,rowSpan=0,columnSpan=0;
      grid->getItemPosition(grid->indexOf(hostDate),&row,&column,&rowSpan,&columnSpan);
      QCOMPARE(row,2);QCOMPARE(column,0);
      grid->getItemPosition(grid->indexOf(date),&row,&column,&rowSpan,&columnSpan);
      QCOMPARE(row,2);QCOMPARE(column,2);
      QCOMPARE(date->minimumDate(),QDate(2000,1,1));
      QCOMPARE(date->maximumDate(),QDate(2099,12,31));
      auto *copy=dialog->findChild<QPushButton*>("clockHostToRnet");
      QVERIFY(copy);
      QVERIFY(!copy->isEnabled());
      auto *set=dialog->findChild<QPushButton*>("setRnetClockButton");
      QVERIFY(set);
      QVERIFY(!set->isEnabled());
      QVERIFY(dialog->grab().save(root_+"/analysis/rnet-clock-dialog.png"));
      dialog->reject();
    });
    action->trigger();
    QVERIFY(dialogSeen);
    QVERIFY(!w->pendingRnetClock_.isValid());
  }
  void distanceMenuOpensWithoutSending(){
    auto w=make();
    languages().setLanguage("de_DE");
    auto *action=w->findChild<QAction*>("command33182");
    QVERIFY(action&&action->isEnabled());
    bool seen=false;
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      seen=dialog->objectName()=="distanceDialog";
      QCOMPARE(dialog->findChild<QLabel*>("totalDistance")->text(),QString("—"));
      QCOMPARE(dialog->findChild<QLabel*>("tripDistance")->text(),QString("—"));
      QVERIFY(!dialog->findChild<QPushButton*>("clearTripDistance")->isEnabled());
      QVERIFY(!dialog->findChild<QPushButton*>("clearTotalDistance")->isEnabled());
      QVERIFY(dialog->grab().save(root_+"/analysis/rnet-distance-dialog.png"));
      dialog->reject();
    });
    action->trigger();
    QVERIFY(seen);
    QVERIFY(!w->pendingRnetClock_.isValid());
  }
  void systemTimersMenuShowsReadOnlyDialog(){
    auto w=make();
    w->closeLiveCanDump();
    int sockets[2];
    QCOMPARE(::socketpair(AF_UNIX, SOCK_DGRAM|SOCK_NONBLOCK, 0, sockets),0);
    QFile peer;
    QVERIFY(peer.open(sockets[1],QIODevice::ReadOnly,QFileDevice::AutoCloseHandle));
    w->liveDumpFd_=sockets[0];
    w->liveDumpInterface_=w->settings_.canInterface;
    w->canSender_.attach(sockets[0]);
    auto *action=w->findChild<QAction*>("command32979");
    QVERIFY(action&&action->isEnabled());
    bool seen=false;
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      QTimer::singleShot(2500,dialog,&QDialog::reject);
      seen=dialog->objectName()=="systemTimersDialog";
      auto *table=dialog->findChild<QTreeWidget*>("systemTimersTable");
      auto *status=dialog->findChild<QLabel*>("systemTimersStatus");
      auto *stationary=dialog->findChild<QCheckBox*>("systemTimersStationary");
      auto *read=dialog->findChild<QPushButton*>("systemTimersRead");
      QVERIFY(table&&status&&stationary&&read);
      QCOMPARE(table->columnCount(),3);
      QCOMPARE(table->topLevelItemCount(),15);
      for (int row=0;row<15;++row) {
        QCOMPARE(table->topLevelItem(row)->text(0),QString::number(row));
        QCOMPARE(table->topLevelItem(row)->text(1),QString("Not queried"));
        QCOMPARE(table->topLevelItem(row)->text(2),QString("Not queried"));
      }
      QVERIFY(!read->isEnabled());
      can_frame request{};
      QCOMPARE(::recv(sockets[1],&request,sizeof(request),0),ssize_t(-1));
      stationary->setChecked(true);
      read->click();
      for (unsigned node=0;node<15;++node) {
        QCOMPARE(::recv(sockets[1],&request,sizeof(request),0),ssize_t(sizeof(request)));
        QCOMPARE(request.can_id,0x78fu);
        QCOMPARE(request.can_dlc,8);
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(request.data),8),
                 QByteArray::fromHex("4010020000000000").replace(0,1,QByteArray(1,char(0x40|node))));
        if (node==3) {
          QTRY_COMPARE_WITH_TIMEOUT(table->topLevelItem(node)->text(2),QString("No reply"),1000);
        } else {
          can_frame reply=request;
          reply.can_id=0x790+node;
          reply.data[0]=node<2 ? 0x2f : 0xcf;
          if (node==0)
            reply.data[4]=125;
          if (node==1) {
            reply.data[4]=reply.data[5]=reply.data[6]=0xff;
            reply.data[7]=0x7f;
          }
          if (node>=2)
            reply.data[4]=0x20;
          QCOMPARE(::send(sockets[1],&reply,sizeof(reply),0),ssize_t(sizeof(reply)));
          w->receiveLiveCanDump();
        }
        QCOMPARE(::recv(sockets[1],&request,sizeof(request),0),ssize_t(sizeof(request)));
        QCOMPARE(request.can_id,0x78fu);
        QCOMPARE(request.can_dlc,8);
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(request.data),8),
                 QByteArray::fromHex("40C7000000000000").replace(0,1,QByteArray(1,char(0x40|node))));
        can_frame reply=request;
        reply.can_id=0x790+node;
        reply.data[0]=node<4 ? 0x2f : 0xcf;
        const unsigned char moduleTypes[]{1,3,4,2};
        reply.data[4]=node<4 ? moduleTypes[node] : 0x20;
        QCOMPARE(::send(sockets[1],&reply,sizeof(reply),0),ssize_t(sizeof(reply)));
        w->receiveLiveCanDump();
      }
      QCOMPARE(table->topLevelItemCount(),15);
      QCOMPARE(table->topLevelItem(0)->text(1),QString("PM"));
      QCOMPARE(table->topLevelItem(1)->text(1),QString("ISM"));
      QCOMPARE(table->topLevelItem(2)->text(1),QString("Dual"));
      QCOMPARE(table->topLevelItem(3)->text(1),QString("JSM"));
      QCOMPARE(table->topLevelItem(14)->text(0),QString("14"));
      QCOMPARE(table->topLevelItem(0)->text(2),QString("2 h 5 min"));
      QCOMPARE(table->topLevelItem(1)->text(2),QString("Timer unavailable"));
      QCOMPARE(table->topLevelItem(2)->text(2),QString("Query rejected (0x20)"));
      QCOMPARE(table->topLevelItem(3)->text(2),QString("No reply"));
      QCOMPARE(table->topLevelItem(14)->text(2),QString("Query rejected (0x20)"));
      QCOMPARE(table->topLevelItem(14)->text(1),QString("Query rejected (0x20)"));
      QCOMPARE(status->text(),QString("Timer read complete."));
      QCOMPARE(::recv(sockets[1],&request,sizeof(request),0),ssize_t(-1));
      QVERIFY(dialog->grab().save(root_+"/analysis/rnet-system-timers-dialog.png"));
      dialog->reject();
    });
    action->trigger();
    QVERIFY(seen);
    QVERIFY(!w->systemTimerReply_);
  }
  void diagnosticsMenuOpensDialog(){
    auto w=make();
    auto *action=w->findChild<QAction*>("command32965");
    QVERIFY(action&&action->isEnabled());
    can_frame fault{};
    fault.can_id=CAN_EFF_FLAG|CAN_RTR_FLAG|0x1c200300;
    fault.can_dlc=5;
    fault.data[0]=0x04;
    fault.data[1]=0x81;
    fault.data[4]=0x03;
    w->observeDiagnostics(fault);
    QVERIFY(!w->diagnosticReceivedAt_.isValid());
    fault.can_id &= ~CAN_RTR_FLAG;
    w->observeDiagnostics(fault);
    QVERIFY(w->diagnosticReceivedAt_.isValid());
    QTimer watchdog;
    watchdog.setSingleShot(true);
    connect(&watchdog,&QTimer::timeout,w.get(),[]{
      if (auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        dialog->reject();
    });
    bool seen=false;
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog)
        return;
      auto *status=dialog->findChild<QLabel*>("diagnosticsStatus");
      seen=dialog->objectName()=="diagnosticsDialog" && status &&
           status->text().contains("8104") && status->text().contains("1C200300#04 81 00 00 03");
      const auto screenshot=dialog->grab();
      dialog->reject();
      QVERIFY(screenshot.save(root_+"/analysis/rnet-diagnostics.png"));
    });
    watchdog.start(1000);
    action->trigger();
    QVERIFY(seen);
  }
  void systemLogsMenuReadsPowerModuleEntries(){
    auto w=make();
    w->closeLiveCanDump();
    int sockets[2];
    QCOMPARE(::socketpair(AF_UNIX, SOCK_DGRAM|SOCK_NONBLOCK, 0, sockets),0);
    QFile peer;
    QVERIFY(peer.open(sockets[1],QIODevice::ReadOnly,QFileDevice::AutoCloseHandle));
    w->liveDumpFd_=sockets[0];
    w->liveDumpInterface_=w->settings_.canInterface;
    w->canSender_.attach(sockets[0]);
    auto *action=w->findChild<QAction*>("command32876");
    QVERIFY(action&&action->isEnabled());
    w->show();
    action->trigger();
    auto *dock=w->findChild<QWidget*>("systemLogsDock");
    QVERIFY(dock&&dock->isVisible());
    auto *read=dock->findChild<QPushButton*>("systemLogsRead");
    auto *status=dock->findChild<QLabel*>("systemLogsStatus");
    auto *entries=dock->findChild<QListWidget*>("systemLogsEntries");
    QVERIFY(read&&status&&entries);
    can_frame frame{};
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(-1));
    read->click();
    QVERIFY(!read->isEnabled());
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(sizeof(frame)));
    QCOMPARE(frame.can_id,0x78fu);
    QCOMPARE(frame.can_dlc,8);
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(frame.data),8).toHex(),
             QByteArray("2000020000000000"));
    frame.can_id=0x790;
    frame.data[0]=0x4f;
    frame.data[2]=0x03;
    w->receiveSystemLogsReply(frame);
    QVERIFY(!read->isEnabled());
    frame.data[2]=0x02;
    w->receiveSystemLogsReply(frame);
    QVERIFY(!read->isEnabled());
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(sizeof(frame)));
    QCOMPARE(frame.can_id,0x78fu);
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(frame.data),8).toHex(),
             QByteArray("4002020000000000"));
    frame.can_id=0x790;
    frame.data[0]=0x2f;
    frame.data[4]=0x04;
    frame.data[5]=0x81;
    frame.data[6]=0x03;
    frame.data[7]=0x07;
    w->receiveSystemLogsReply(frame);
    QCOMPARE(entries->count(),1);
    QCOMPARE(entries->item(0)->text(),QString("8104 · Unexpected CAN payload length · 7×"));
    QCOMPARE(entries->item(0)->toolTip(),QString("04 81 03 07"));
    QVERIFY(!read->isEnabled());
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(sizeof(frame)));
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(frame.data),8).toHex(),
             QByteArray("2000020001000000"));
    frame.can_id=0x790;
    frame.data[0]=0x4f;
    frame.data[4]=0;
    w->receiveSystemLogsReply(frame);
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(sizeof(frame)));
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(frame.data),8).toHex(),
             QByteArray("4002020000000000"));
    frame.can_id=0x790;
    frame.data[0]=0x2f;
    w->receiveSystemLogsReply(frame);
    QVERIFY(read->isEnabled());
    QCOMPARE(entries->count(),1);
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),0),ssize_t(-1));
    QVERIFY(w->grab().save(root_+"/analysis/rnet-system-logs.png"));
  }
  void distanceMenuShowsReceivedValues(){
    auto w=make();
    languages().setLanguage("de_DE");
    w->rnetDistance_=decodeRnetDistanceFrame(CAN_EFF_FLAG|rnetDistanceBroadcastId,
                                             QByteArray::fromHex("F2F60500F2F60500"));
    QVERIFY(w->rnetDistance_.valid);
    w->rnetDistanceAge_.restart();
    w->liveDumpInterface_=w->settings_.canInterface;
    w->liveDumpFd_=::socket(AF_UNIX,SOCK_DGRAM,0); // Offline UI fixture; never bound to CAN.
    QVERIFY(w->liveDumpFd_>=0);
    w->canSender_.attach(w->liveDumpFd_);
    bool seen=false;
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      seen=dialog->objectName()=="distanceDialog";
      QCOMPARE(dialog->findChild<QLabel*>("totalDistance")->text(),QLocale().toString(qulonglong(390898)));
      QCOMPARE(dialog->findChild<QLabel*>("tripDistance")->text(),QLocale().toString(qulonglong(390898)));
      QVERIFY(dialog->findChild<QPushButton*>("clearTripDistance")->isEnabled());
      QVERIFY(dialog->findChild<QPushButton*>("clearTotalDistance")->isEnabled());
      QVERIFY(dialog->grab().save(root_+"/analysis/rnet-distance-live-dialog.png"));
      dialog->reject();
    });
    w->findChild<QAction*>("command33182")->trigger();
    QVERIFY(seen);
  }
  void distanceClearMatchesOriginalZeroLengthFrame(){
    auto w=make();
    int sockets[2]={-1,-1};
    QVERIFY(::socketpair(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK,0,sockets)==0);
    w->liveDumpFd_=sockets[0];w->canSender_.attach(sockets[0]);
    w->liveDumpInterface_=w->settings_.canInterface;
    w->rnetDistance_={390898,390898,true};w->rnetDistanceAge_.restart();
    w->clearRnetDistance(false);
    can_frame frame{};
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),MSG_DONTWAIT),ssize_t(sizeof(frame)));
    QCOMPARE(frame.can_id,canid_t(CAN_EFF_FLAG|rnetClearTripId));
    QCOMPARE(frame.can_dlc,quint8(0));
    can_frame readback{};readback.can_id=CAN_EFF_FLAG|rnetDistanceBroadcastId;readback.can_dlc=8;
    const auto tripCleared=QByteArray::fromHex("F2F6050000000000");
    std::copy(tripCleared.begin(),tripCleared.end(),readback.data);
    QCOMPARE(::write(sockets[1],&readback,sizeof(readback)),ssize_t(sizeof(readback)));
    w->receiveLiveCanDump();
    QCOMPARE(w->pendingDistanceReset_,0);
    QCOMPARE(w->rnetDistance_.tripMetres,quint32(0));
    w->clearRnetDistance(true);
    QCOMPARE(::recv(sockets[1],&frame,sizeof(frame),MSG_DONTWAIT),ssize_t(sizeof(frame)));
    QCOMPARE(frame.can_id,canid_t(CAN_EFF_FLAG|rnetClearTotalId));
    QCOMPARE(frame.can_dlc,quint8(0));
    const auto totalCleared=QByteArray::fromHex("0000000000000000");
    std::copy(totalCleared.begin(),totalCleared.end(),readback.data);
    QCOMPARE(::write(sockets[1],&readback,sizeof(readback)),ssize_t(sizeof(readback)));
    w->receiveLiveCanDump();
    QCOMPARE(w->pendingDistanceReset_,0);
    QCOMPARE(w->rnetDistance_.totalMetres,quint32(0));
    ::close(sockets[1]);
  }
  void oneSocketFeedsAllWidgetsAndClockSend(){
    auto w=make();
    QVERIFY(w->liveDumpFd_<0);
    int sockets[2]={-1,-1};
    QVERIFY(::socketpair(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK,0,sockets)==0);
    w->liveDumpFd_=sockets[0];
    w->canSender_.attach(w->liveDumpFd_);
    w->liveDumpInterface_=w->settings_.canInterface;
    w->heartbeat_->setSocketState(true);
    w->monitorActive_=true;
    w->monitorTimer_=new QTimer(w.get());
    const auto inject=[&](quint32 id,const QByteArray &payload){
      can_frame frame{};frame.can_id=CAN_EFF_FLAG|id;frame.can_dlc=quint8(payload.size());
      std::copy(payload.begin(),payload.end(),frame.data);
      QCOMPARE(::write(sockets[1],&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    };
    inject(0x03c30f0fU,QByteArray(7,char(0x87)));
    inject(rnetDistanceBroadcastId,QByteArray::fromHex("F2F60500F2F60500"));
    inject(rnetClockBroadcastId,QByteArray::fromHex("281604ef0b1a"));
    w->receiveLiveCanDump();
    QVERIFY(w->heartbeat_->receiving());
    QVERIFY(w->rnetDistance_.valid);
    QCOMPARE(w->rnetDistance_.totalMetres,quint32(390898));
    QVERIFY(w->rnetClock_.isValid());
    QCOMPARE(w->frameCount_,quint64(3));
    w->stopMonitor(false);
    w->startBlinkCycle();
    can_frame sent{};
    QCOMPARE(::recv(sockets[1],&sent,sizeof(sent),MSG_DONTWAIT),ssize_t(sizeof(sent)));
    QCOMPARE(sent.can_id,canid_t(CAN_EFF_FLAG|0x0c000403U));
    w->blinkCycle_->quiesce();
    QCOMPARE(::recv(sockets[1],&sent,sizeof(sent),MSG_DONTWAIT),ssize_t(sizeof(sent)));
    QCOMPARE(sent.can_id,canid_t(CAN_EFF_FLAG|0x0c000403U));
    w->blinkCycle_->stop();
    w->setRnetClock(QDateTime(QDate(2026,11,17),QTime(12,30,0)),false);
    QCOMPARE(::recv(sockets[1],&sent,sizeof(sent),MSG_DONTWAIT),ssize_t(sizeof(sent)));
    QCOMPARE(sent.can_id,canid_t(CAN_EFF_FLAG|rnetClockSetId));
    QCOMPARE(sent.can_dlc,quint8(6));
    ::close(sockets[1]);
  }
  void clockSettingsManualTimeNeedsNoStationaryPrompt() {
    auto w=make();
    languages().setLanguage("de_DE");
    const auto host=QDateTime::currentDateTime();
    w->rnetClock_={host.date(),host.time()};
    w->rnetClockAge_.restart();
    QTimer::singleShot(0,w.get(),[&]{
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      auto *date=dialog->findChild<QDateEdit*>("clockRnetDate");
      auto *time=dialog->findChild<QTimeEdit*>("clockRnetTime");
      auto *hostTime=dialog->findChild<QTimeEdit*>("clockComputerTime");
      auto *set=dialog->findChild<QPushButton*>("setRnetClockButton");
      auto *copy=dialog->findChild<QPushButton*>("clockHostToRnet");
      QVERIFY(date&&time&&hostTime&&set&&copy);
      QCOMPARE(hostTime->displayFormat(),QString("HH:mm:ss"));
      QCOMPARE(time->displayFormat(),QString("HH:mm:ss"));
      QCOMPARE(date->displayFormat(),QString("dd.MM.yyyy"));
      QVERIFY(set->isEnabled());
      QVERIFY(copy->isEnabled());
      date->setDate(QDate(2030,4,5));
      time->setTime(QTime(12,34,56));
      QVERIFY(dialog->grab().save(root_+"/analysis/rnet-clock-settings.png"));
      dialog->reject();
    });
    w->findChild<QAction*>("command33051")->trigger();
    QVERIFY_THROWS_EXCEPTION(UiError,w->setRnetClock(QDateTime(QDate(2030,4,5),QTime(12,34,56)),false));
    QVERIFY(QApplication::activeModalWidget()==nullptr);
    QVERIFY(!w->pendingRnetClock_.isValid());
  }
  void typedEditorsValidateOriginalMappings() {
    auto w=make();w->openDocument(root_+"/tests/fixtures/pid68-before-full.R-net");
    const auto original=w->document()->write();
    QCOMPARE(w->catalog_.editors.size(),std::size_t(164));
    auto find=[&](unsigned pid){for(const auto &v:w->values_)if(v.parameter==pid)return v;return DocumentValue{};};
    auto boolean=find(68);QVERIFY(boolean.parameter==68);
    ParameterValueEditor yesno(w->catalog_.parameters.at(68),boolean,w->catalog_);
    auto *combo=yesno.findChild<QComboBox*>("choiceValueEditor");QVERIFY(combo);QCOMPARE(combo->count(),2);
    QCOMPARE(combo->itemText(0),QString("No (0)"));combo->setCurrentIndex(1);QCOMPARE(yesno.rawValue(),QString("1"));QVERIFY(yesno.valid());
    languages().setLanguage("de_DE");
    ParameterValueEditor deutsch(w->catalog_.parameters.at(68),boolean,w->catalog_);
    QCOMPARE(deutsch.findChild<QComboBox*>()->itemText(1),QString("Ja (1)"));
    languages().setLanguage("en_US");
    auto assignment=find(810);QVERIFY(assignment.parameter==810);
    ParameterValueEditor enumEditor(w->catalog_.parameters.at(810),assignment,w->catalog_);
    auto *select=enumEditor.findChild<QComboBox*>();QVERIFY(select);
    QCOMPARE(select->currentData().toLongLong(),qlonglong(-1));QVERIFY(enumEditor.valid());
    auto enumChange=*w->document();
    QVERIFY(edit_value(enumChange,w->catalog_,assignment,u"0",AccessLevel::OEM));
    QCOMPARE(controller_write_plan(*w->document(),enumChange,w->catalog_,AccessLevel::OEM).size(),std::size_t(1));

    bool holeFound=false;
    for(const auto &v:w->values_){
      const auto &p=w->catalog_.parameters.at(v.parameter);auto choices=parameter_choices(p,v,w->catalog_);
      if(choices.size()<2)continue;
      auto low=p.signed_storage?qint64(qint32(p.metadata[4])):qint64(p.metadata[4]);
      auto high=p.signed_storage?qint64(qint32(p.metadata[5])):qint64(p.metadata[5]);
      for(qint64 value=low;value<=high&&value-low<256;++value){
        bool present=false;for(const auto &choice:choices)present|=choice.value==value;
        if(present)continue;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,encode_parameter_value(p,v,QString::number(value).toStdU16String(),w->catalog_));
        holeFound=true;break;
      }
      if(holeFound)break;
    }
    QVERIFY(holeFound);
    auto number=find(312);auto definition=w->catalog_.parameters.at(312);definition.metadata[4]=0;definition.metadata[5]=100;definition.metadata[6]=5;
    ParameterValueEditor numeric(definition,number,w->catalog_);auto *line=numeric.findChild<QLineEdit*>("numberValueEditor");QVERIFY(line);
    line->setText("101");QVERIFY(!numeric.valid());line->setText("51");QVERIFY(!numeric.valid());line->setText("50");QVERIFY(numeric.valid());
    numeric.findChild<QToolButton*>("stepUp")->click();QCOMPARE(numeric.rawValue(),QString("55"));
    definition.metadata[4]=0;definition.metadata[5]=0xffffffffu;definition.metadata[6]=1;number.width=4;
    ParameterValueEditor wide(definition,number,w->catalog_);wide.findChild<QLineEdit*>()->setText("4294967295");QVERIFY(wide.valid());
    wide.findChild<QToolButton*>("stepUp")->click();QCOMPARE(wide.rawValue(),QString("4294967295"));
    auto text=find(306);QVERIFY(text.parameter==306);
    ParameterValueEditor textual(w->catalog_.parameters.at(306),text,w->catalog_);
    auto *name=textual.findChild<QLineEdit*>("textValueEditor");QVERIFY(name);QCOMPARE(name->maxLength(),int(text.width));
    name->setText(QString::fromUtf8("Größe €"));QVERIFY(textual.valid());name->setText(QString::fromUtf8("🙂"));QVERIFY(!textual.valid());
    QCOMPARE(w->document()->write(),original);
    QDialog preview;preview.setWindowTitle("R-net · Parameter-specific raw value editors");
    auto *form=new QFormLayout(&preview);
    form->addRow("Yes / No",new ParameterValueEditor(w->catalog_.parameters.at(68),boolean,w->catalog_,&preview));
    form->addRow("Selection with explicit raw values",new ParameterValueEditor(w->catalog_.parameters.at(810),assignment,w->catalog_,&preview));
    form->addRow("Number: range and step",new ParameterValueEditor(w->catalog_.parameters.at(312),find(312),w->catalog_,&preview));
    form->addRow("Text: length and encoding",new ParameterValueEditor(w->catalog_.parameters.at(306),text,w->catalog_,&preview));
    preview.resize(640,230);preview.show();QTest::qWait(20);
    QVERIFY(preview.grab().save(root_+"/analysis/parameter-editors-preview.png"));
  }
  void inlineRawValueEditorCommitCancelAndBounds() {
    auto w=make();w->openDocument(root_+"/tests/fixtures/pid68-before-full.R-net");
    auto original=w->document()->write();
    auto *table=w->findChild<QTableView*>("parameters");
    auto *search=w->findChild<QLineEdit*>("search");search->setText("Keycode");QCOMPARE(w->filter()->rowCount(),1);
    auto index=w->filter()->index(0,2);QVERIFY(index.flags()&Qt::ItemIsEditable);
    table->setCurrentIndex(index);table->setFocus();QTest::keyClick(table,Qt::Key_F2);
    QTRY_VERIFY(table->findChild<QComboBox*>("choiceValueEditor"));
    auto *combo=table->findChild<QComboBox*>("choiceValueEditor");combo->setCurrentIndex(1);QTest::keyClick(combo,Qt::Key_Return);
    QTRY_VERIFY(w->isWindowModified());QCOMPARE(w->filter()->data(w->filter()->index(0,2)).toString(),QString("1"));
    QTRY_VERIFY(!table->findChild<QWidget*>("parameterValueEditor"));
    w->undo();search->setText("Keycode");index=w->filter()->index(0,2);table->setCurrentIndex(index);table->setFocus();QTest::keyClick(table,Qt::Key_F2);
    QTRY_VERIFY(table->findChild<QComboBox*>("choiceValueEditor"));combo=table->findChild<QComboBox*>("choiceValueEditor");combo->setCurrentIndex(1);QTest::keyClick(combo,Qt::Key_Escape);
    QCOMPARE(w->document()->write(),original);
    w->saveAs(temp_.path()+"/cancelled-editor.R-net");
    QCOMPARE(w->document()->write(),original);
    QTRY_VERIFY(!table->findChild<QWidget*>("parameterValueEditor"));
    search->setText("Maximum Forward Speed");
    index=w->filter()->index(0,2);table->setCurrentIndex(index);table->setFocus();QTest::keyClick(table,Qt::Key_F2);
    QTRY_VERIFY(table->findChild<QLineEdit*>("numberValueEditor"));
    auto *line=table->findChild<QLineEdit*>("numberValueEditor");line->setText("101");QTest::keyClick(line,Qt::Key_Return);
    QCOMPARE(w->document()->write(),original);QVERIFY(table->findChild<QWidget*>("parameterValueEditor"));
    line->setText("86");QTest::keyClick(line,Qt::Key_Return);QTRY_VERIFY(w->isWindowModified());
    QCOMPARE(search->text(),QString("Maximum Forward Speed"));w->undo();
    QCOMPARE(w->document()->write(),original);
    QTRY_VERIFY(!table->findChild<QWidget*>("parameterValueEditor"));
    search->setText("Keycode");
    index=w->filter()->index(0,2);table->setCurrentIndex(index);table->setFocus();QTest::keyClick(table,Qt::Key_F2);
    QTRY_VERIFY(table->findChild<QComboBox*>("choiceValueEditor"));combo=table->findChild<QComboBox*>("choiceValueEditor");combo->setCurrentIndex(1);
    w->saveAs(temp_.path()+"/pending-editor.R-net");
    QVERIFY(w->document()->write()!=original);w->undo();QCOMPARE(w->document()->write(),original);
    w->setAccess(AccessLevel::Therapist);QCOMPARE(w->filter()->rowCount(),0);
    QVERIFY(w->process_==nullptr);
  }
  void acknowledgedWriteWithoutReadbackStaysUnverified() {
    auto w=make();w->openDocument(root_+"/tests/fixtures/pid68-before-full.R-net");
    const auto bytes=w->document()->write();
    w->writeTask_=true;w->process_=new QProcess(w.get());
    w->processText_="COMMIT_ACKNOWLEDGED_READBACK_PENDING\nFAILED: readback mismatch\n";
    bool explained=false;
    try{w->processFinished(2,QProcess::NormalExit);}catch(const UiError &e){
      explained=QString::fromUtf8(e.what()).contains("acknowledged writing") &&
                QString::fromUtf8(e.what()).contains("Do not write again");
    }
    QVERIFY(explained);QVERIFY(w->process_==nullptr);
    QCOMPARE(w->document()->write(),bytes);
    QVERIFY(!w->documentPath().endsWith("/verified.R-net"));
  }
  void changedValuesAndControllerWriteGuards() {
    auto w=make();
    w->openDocument(root_+"/tests/fixtures/pid68-before-full.R-net");
    auto original=*w->document();
    auto *action=w->findChild<QAction *>("command32804");
    QVERIFY(action && action->isEnabled());
    int value=-1;
    for (int i=0;i<w->valueCount();++i) if(w->values_[i].parameter==68) value=i;
    QVERIFY(value>=0);
    w->applyEdit(value,"1");
    int row=-1;
    for(int i=0;i<w->model()->rowCount();++i) if(w->model()->rows[i].id=="68") row=i;
    QVERIFY(row>=0);
    auto index=w->model()->index(row,2);
    QCOMPARE(w->model()->data(index,Qt::ForegroundRole).value<QColor>(),QColor(Qt::red));
    QVERIFY(w->model()->data(index,Qt::FontRole).value<QFont>().bold());
    auto files=controller_write_plan(original,*w->document(),w->catalog_,AccessLevel::OEM);
    QCOMPARE(files.size(),std::size_t(1));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,controller_write_plan(original,*w->document(),w->catalog_,AccessLevel::Therapist));
    auto bad=*w->document();bad.comments.push_back(u"unauthorized metadata");
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,controller_write_plan(original,bad,w->catalog_,AccessLevel::Factory));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->startControllerWrite("can0",temp_.path()+"/never",false));
    QVERIFY(!QFileInfo::exists(temp_.path()+"/never"));
    w->setAccess(AccessLevel::Therapist);
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->startControllerWrite("can0",temp_.path()+"/never",true));
    QVERIFY(!QFileInfo::exists(temp_.path()+"/never"));
    w->setAccess(AccessLevel::OEM);
    w->saveAs(temp_.path()+"/red-saved.R-net");
    w->showValues();
    QVERIFY(w->model()->rows[row].changed);
    w->undo();
    QVERIFY(!w->model()->rows[row].changed);
    w->applyEdit(value,"1");w->applyEdit(value,"0");
    QVERIFY(!w->model()->rows[row].changed);
    // A different supported parameter in another file uses the same plan.
    int speed=-1;
    for(int i=0;i<w->valueCount();++i) if(w->values_[i].parameter==312 && w->values_[i].profile==0) {speed=i;break;}
    QVERIFY(speed>=0);
    w->applyEdit(speed,w->values_[speed].number==86?"85":"86");
    w->applyEdit(value,"1");
    QCOMPARE(controller_write_plan(original,*w->document(),w->catalog_,AccessLevel::OEM).size(),std::size_t(2));
    QVERIFY(w->process_==nullptr);
    w->undo();w->undo();
  }
  void vcanControllerWrite() {
    if(!qEnvironmentVariableIsSet("RNET_QT_WRITE_TEST_DIR")) QSKIP("Private vcan harness only");
    auto w=make();w->openDocument(root_+"/tests/fixtures/pid68-before-full.R-net");
    int value=-1;for(int i=0;i<w->valueCount();++i) if(w->values_[i].parameter==68) value=i;
    QVERIFY(value>=0);w->applyEdit(value,"1");
    w->startControllerWrite("rnetw0",qEnvironmentVariable("RNET_QT_WRITE_TEST_DIR")+"/qt-write",true);
    QVERIFY(!w->findChild<QAction *>("command32804")->isEnabled());
    QVERIFY(!w->findChild<QAction *>("role0")->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(w->process_==nullptr,15000);
    QVERIFY(w->processText_.contains("COMPLETE write verified_files=1"));
    QCOMPARE(w->values_[value].number,std::int64_t(1));
    for(const auto &row:w->model()->rows) QVERIFY(!row.changed);
    QVERIFY(w->findChild<QAction *>("command32804")->isEnabled());
    QVERIFY(w->documentPath().endsWith("/output/verified.R-net"));
  }
  void accessColumnFiltersAllLevels() {
    auto w = make();
    w->openDocument(root_ + "/tests/fixtures/pid68-before-full.R-net");
    auto original = w->document()->write();
    QCOMPARE(w->model()->columnCount(), 6);
    const QColor colors[]{QColor("#FFEB3B"), QColor("#4CAF50"),
                           QColor("#1976D2"), QColor("#D32F2F")};
    int last = -1;
    for (unsigned level = 0; level < 4; ++level) {
      if(level>=2)QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Yes)->click();});
      w->findChild<QAction *>("role" + QString::number(level))->trigger();
      int count = w->filter()->rowCount();
      QVERIFY(count >= last);
      last = count;
      for (int row = 0; row < count; ++row) {
        auto index = w->filter()->mapToSource(w->filter()->index(row, 5));
        const auto &r = w->model()->rows.at(index.row());
        QVERIFY(r.requiredAccess >= 0 && r.requiredAccess <= int(level));
        QCOMPARE(w->model()->data(index, Qt::BackgroundRole).value<QColor>(),
                 colors[r.requiredAccess]);
        QVERIFY(w->model()->data(index).toString().startsWith(QString::number(r.requiredAccess)));
      }
    }
    w->setAccess(AccessLevel::Therapist);
    w->findChild<QLineEdit *>("search")->setText("Keycode");
    QCOMPARE(w->filter()->rowCount(), 0);
    w->setAccess(AccessLevel::Dealer);
    QCOMPARE(w->filter()->rowCount(), 1);
    languages().setLanguage("de_DE");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QCOMPARE(w->model()->headings[5], QString("Berechtigung"));
    QVERIFY(w->filter()->data(w->filter()->index(0, 5)).toString().contains("Händler"));
    QCOMPARE(w->document()->write(), original);
    QVERIFY(w->process_ == nullptr);
    languages().setLanguage("en_US");
  }
  void actualMenuMouseSelection() {
    auto w = make();
    auto *menu = w->findChild<QMenu *>("dongleMenu");
    QVERIFY(menu);
    auto *bar = w->menuBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                      bar->actionGeometry(menu->menuAction()).center());
    QTRY_VERIFY(menu->isVisible());
    auto *dealer = w->findChild<QAction *>("role1");
    QVERIFY(dealer);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                      menu->actionGeometry(dealer).center());
    QCOMPARE(w->access(), AccessLevel::Dealer);
    QVERIFY(dealer->isChecked());
    QVERIFY(!menu->isVisible());
  }
  void roundtripEditUndoAndAtomicSave() {
    auto w = make();
    auto source = read_file(fixture_.toStdString());
    w->openDocument(fixture_);
    QCOMPARE(w->warningCount(), 1);
    QVERIFY(w->valueCount() > 2000);
    QVERIFY(w->document()->write() == source);
    int index = firstSpeed(*w);
    QVERIFY(index >= 0);
    w->applyEdit(index, "86");
    QVERIFY(w->isWindowModified());
    auto edited = w->document()->write();
    QVERIFY(edited != source);
    auto destination = temp_.path() + "/qt-work.R-net";
    w->saveAs(destination);
    QVERIFY(read_file(destination.toStdString()) == edited);
    w->applyEdit(index, "87");
    w->save();
    QVERIFY(read_file(destination.toStdString()) == w->document()->write());
    w->undo();
    QVERIFY(w->document()->write() == edited);
    w->undo();
    QVERIFY(w->document()->write() == source);
    QVERIFY(read_file(fixture_.toStdString()) == source);
  }
  void editButtonUsesTableDelegate() {
    auto w = make();
    w->openDocument(fixture_);
    int index = firstSpeed(*w);
    QVERIFY(index >= 0);
    int row = -1;
    for (int i = 0; i < w->model()->rowCount(); ++i)
      if (w->model()->rows[std::size_t(i)].valueIndex == index)
        row = i;
    QVERIFY(row >= 0);
    auto *table = w->findChild<QTableView *>("parameters");
    table->setCurrentIndex(
        w->filter()->mapFromSource(w->model()->index(row, 0)));
    auto *button = w->findChild<QPushButton *>("editValue");
    QVERIFY(button->isEnabled());
    QTest::mouseClick(button, Qt::LeftButton);
    QTRY_VERIFY(table->findChild<QLineEdit*>("numberValueEditor"));
    auto *editor=table->findChild<QLineEdit*>("numberValueEditor");
    editor->setText("86");
    QTest::keyClick(editor,Qt::Key_Return);
    QTRY_VERIFY(w->isWindowModified());
    QTRY_VERIFY(!table->findChild<QWidget*>("parameterValueEditor"));
    w->undo();
    QVERIFY(!w->isWindowModified());
  }
  void pid68BooleanEditRemainsLocal() {
    auto w = make();
    auto source = root_ + "/tests/fixtures/pid68-before-full.R-net";
    auto before = read_file(source.toStdString());
    w->openDocument(source);
    int row = -1;
    for (int i = 0; i < w->model()->rowCount(); ++i)
      if (w->model()->rows[std::size_t(i)].id == "68")
        row = i;
    QVERIFY(row >= 0);
    QCOMPARE(w->model()->rows[std::size_t(row)].value, QString("0"));
    auto *table = w->findChild<QTableView *>("parameters");
    table->setCurrentIndex(
        w->filter()->mapFromSource(w->model()->index(row, 0)));
    auto *edit = w->findChild<QPushButton *>("editValue");
    QVERIFY(edit->isEnabled());
    QTest::mouseClick(edit, Qt::LeftButton);
    QTRY_VERIFY(table->findChild<QComboBox*>("choiceValueEditor"));
    auto *choice=table->findChild<QComboBox*>("choiceValueEditor");
    choice->setCurrentIndex(choice->findData(QVariant::fromValue(qlonglong(1))));
    QTest::keyClick(choice,Qt::Key_Return);
    QTRY_VERIFY(w->isWindowModified());
    QCOMPARE(w->model()->rows[std::size_t(row)].value, QString("1"));
    QVERIFY(w->process_ == nullptr);
    w->undo();
    QVERIFY(w->document()->write() == before);
    QVERIFY(read_file(source.toStdString()) == before);
  }
  void fileProtectionAndFailedOpen() {
    auto w = make();
    w->openDocument(fixture_);
    auto source = w->document()->write();
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->saveAs(fixture_));
    QVERIFY(w->document()->write() == source);
    auto invalid = temp_.path() + "/invalid.R-net";
    QFile f(invalid);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("invalid");
    f.close();
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->openDocument(invalid));
    QVERIFY(w->document()->write() == source);
    int index = firstSpeed(*w);
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                             w->applyEdit(index, "999999999999999999999999"));
    QVERIFY(w->document()->write() == source);
    auto destination = temp_.path() + "/externally-modified.R-net";
    w->saveAs(destination);
    QFile external(destination);
    QVERIFY(external.open(QIODevice::WriteOnly));
    external.write("external change");
    external.close();
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->save());
    QVERIFY(external.open(QIODevice::ReadOnly));
    QCOMPARE(external.readAll(), QByteArray("external change"));
  }
  void closeCancelPreservesDirtyDocument() {
    auto w = make();
    w->openDocument(fixture_);
    w->applyEdit(firstSpeed(*w), "86");
    QTimer::singleShot(50, w.get(), [] {
      auto *box =
          qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (box)
        box->button(QMessageBox::Cancel)->click();
    });
    QVERIFY(!w->close());
    QVERIFY(w->isVisible());
    QVERIFY(w->isWindowModified());
    w->undo();
    QVERIFY(w->close());
  }
  void repositoryResourcesAndCsv() {
    auto w = make();
    w->openDocument(fixture_);
    int files = w->fileCount();
    w->showRepository();
    QCOMPARE(w->model()->rowCount(), files);
    w->showValues();
    w->findChild<QLineEdit *>("search")->setText("Maximum Forward Speed");
    QCOMPARE(w->filter()->rowCount(), 8);
    auto target = temp_.path() + "/qt-export.csv";
    w->exportCsv(target);
    QFile f(target);
    QVERIFY(f.open(QIODevice::ReadOnly));
    auto text = f.readAll();
    QCOMPARE(text.count('\n'), 9);
    QVERIFY(text.contains("Maximum Forward Speed"));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,w->exportCsv(target));
    w->showResources();
    QCOMPARE(w->model()->rowCount(), 1454);
  }
  void actualSnapshotPreservesOpaqueFiles() {
    auto file = root_ + "/analysis/controller-config-3ng708np/snapshot/"
                        "AX23021030-configuration.R-net";
    if (!QFileInfo::exists(file))
      QSKIP("Private live snapshot unavailable");
    auto w = make();
    auto bytes = read_file(file.toStdString());
    w->openDocument(file);
    QCOMPARE(w->fileCount(), 23);
    QCOMPARE(w->valueCount(), 2331);
    QCOMPARE(w->warningCount(), 3);
    QVERIFY(w->document()->write() == bytes);
    auto target = temp_.path() + "/qt-live-copy.R-net";
    w->saveAs(target);
    QVERIFY(read_file(target.toStdString()) == bytes);
    QVERIFY(w->findChild<QLabel *>("provenanceBanner")
                ->text()
                .contains("original default template"));
  }
  void configurationOnPrivateVcan() {
    if (qEnvironmentVariable("RNET_QT_CONFIG_CAN") != "rnetcfg0")
      QSKIP("Only run in isolated configuration test namespace");
    auto w = make();
    auto destination = qEnvironmentVariable("RNET_QT_CONFIG_OUTPUT");
    QVERIFY(!destination.isEmpty());
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        w->startControllerRead("rnetcfg0", destination, false));
    QVERIFY(w->process_ == nullptr);
    w->openDocument(fixture_);
    w->applyEdit(firstSpeed(*w), "86");
    int discardPrompts = 0;
    QTimer discard;
    connect(&discard, &QTimer::timeout, w.get(), [&] {
      if (auto *box =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        ++discardPrompts;
        box->button(QMessageBox::Discard)->click();
      }
    });
    discard.start(50);
    w->startControllerRead("rnetcfg0", destination, true);
    QTRY_VERIFY_WITH_TIMEOUT(w->process_ == nullptr, 25000);
    QTextStream(stdout) << w->processText_;
    QVERIFY(w->processText_.contains("RELEASE_VERIFIED node=2 owner_status=0"));
    QVERIFY(w->processText_.contains(
        "COMPLETE mode=read files=20 errors=0 cancelled=0"));
    QCOMPARE(discardPrompts, 1);
    QCOMPARE(w->fileCount(), 20);
    QCOMPARE(w->valueCount(), 2026);
    QVERIFY(QFileInfo::exists(w->documentPath()));
  }
  void monitorOnPrivateVcan() {
    auto interface = qEnvironmentVariable("RNET_QT_TEST_CAN");
    if (interface.isEmpty())
      QSKIP("Only run in isolated network namespace");
    QVERIFY(interface == "rnetqt0");
    auto w = make();
    w->settings_.canInterface=interface;
    w->applyPreferences();
    QTRY_VERIFY_WITH_TIMEOUT(w->liveDumpFd_>=0,2000);
    w->startMonitor(interface, 300);
    auto *lower=w->findChild<QSplitter*>("detailsTaskSplitter");
    auto *liveDump=w->findChild<QPlainTextEdit*>("liveCanDump");
    QVERIFY(lower&&liveDump&&w->taskPanel_->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(lower->handle(1)->isVisible(),1000);
    const int dumpBottom=liveDump->mapTo(w.get(),QPoint(0,liveDump->height())).y();
    const int taskBottom=w->taskPanel_->mapTo(w.get(),QPoint(0,w->taskPanel_->height())).y();
    QVERIFY(dumpBottom>=taskBottom-2); // The live dump must continue beside the task log.
    languages().setLanguage("de_DE");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QVERIFY(w->monitoring());
    QVERIFY(!w->findChild<QAction *>("command32803")->isEnabled());
    int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    QVERIFY(fd >= 0);
    ifreq request{};
    qstrncpy(request.ifr_name, "rnetqt0", IFNAMSIZ);
    QVERIFY(ioctl(fd, SIOCGIFINDEX, &request) == 0);
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    QVERIFY(::bind(fd, reinterpret_cast<sockaddr *>(&address),
                   sizeof(address)) == 0);
    can_frame frame{};
    frame.can_id = 0x123;
    frame.can_dlc = 3;
    frame.data[0] = 1;
    frame.data[1] = 2;
    frame.data[2] = 3;
    QCOMPARE(::write(fd, &frame, sizeof(frame)), ssize_t(sizeof(frame)));
    QTRY_VERIFY_WITH_TIMEOUT(liveDump->toPlainText().contains("123 [3] 01 02 03"),2000);
    QTRY_VERIFY_WITH_TIMEOUT(!w->monitoring(), 2000);
    QVERIFY(w->liveDumpFd_>=0);
    QVERIFY(w->findChild<QAction *>("command32803")->isEnabled());
    QCOMPARE(w->receivedFrames(), quint64(1));
    QCOMPARE(w->model()->rowCount(), 1);
    QCOMPARE(w->model()->rows[0].value, QString("01 02 03"));
    auto *followMode=w->findChild<QToolButton*>("liveCanViewFollow");
    auto *countMode=w->findChild<QToolButton*>("liveCanViewCount");
    auto *hertzUnit=w->findChild<QToolButton*>("liveCanViewHz");
    auto *msUnit=w->findChild<QToolButton*>("liveCanViewMs");
    QVERIFY(followMode&&countMode&&hertzUnit&&msUnit);
    QVERIFY(followMode->isChecked());
    QVERIFY(hertzUnit->isChecked());
    QCOMPARE(followMode->toolTip(),QString("Verlauf"));
    QCOMPARE(countMode->toolTip(),QString("Zählung"));
    QVERIFY(followMode->text().isEmpty());
    auto *counts=w->findChild<QTableWidget*>("liveCanCount");
    QVERIFY(counts);
    countMode->click();
    QTRY_COMPARE(counts->rowCount(),1);
    QVERIFY(counts->isVisible());QVERIFY(!liveDump->isVisible());
    QCOMPARE(counts->item(0,0)->text(),QString("       1"));
    QCOMPARE(counts->item(0,1)->text(),QString("123"));
    QVERIFY(counts->item(0,1)->textAlignment()&Qt::AlignRight);
    QCOMPARE(counts->columnCount(),4);
    QCOMPARE(counts->horizontalHeaderItem(2)->text(),QString("avg [Hz]"));
    QVERIFY(counts->item(0,2)->text().toDouble()>0.0);
    msUnit->click();
    QCOMPARE(counts->horizontalHeaderItem(2)->text(),QString("avg [ms]"));
    QVERIFY(counts->item(0,2)->text().toDouble()>0.0);
    hertzUnit->click();
    QCOMPARE(counts->horizontalHeaderItem(2)->text(),QString("avg [Hz]"));
    QCOMPARE(counts->item(0,3)->text(),QString("[3] 01 02 03"));
    frame.data[0]=4;
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_COMPARE(counts->item(0,0)->text(),QString("       2"));
    QCOMPARE(counts->item(0,3)->text(),QString("[3] 04 02 03"));
    frame.can_id=0x124;
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_COMPARE(counts->rowCount(),2);
    QCOMPARE(counts->item(1,0)->text(),QString("       1"));
    QCOMPARE(counts->item(1,1)->text(),QString("124"));
    followMode->click();
    QVERIFY(liveDump->isVisible());QVERIFY(!counts->isVisible());
    QCOMPARE(liveDump->toPlainText().split('\n').size(),3);
    QVERIFY(liveDump->toPlainText().contains("123 [3] 01 02 03"));
    QVERIFY(liveDump->toPlainText().contains("123 [3] 04 02 03"));
    languages().setLanguage("en_US");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QCOMPARE(followMode->toolTip(),QString("Follow"));
    QCOMPARE(countMode->toolTip(),QString("Count"));
    for(unsigned command:{32956u,32955u,32957u,33390u,33343u,
                          33208u,33392u,33395u,32817u,33107u}){
      auto *action=w->findChild<QAction*>("command"+QString::number(command));
      QVERIFY(action&&action->isEnabled());
      action->trigger();
      auto *dialog=w->findChild<QDialog*>("systemTestDialog");
      QVERIFY(dialog&&dialog->isVisible());
      QCOMPARE(w->systemTestCommand_,command);
    }
    auto *testDump=w->findChild<QTableWidget*>("systemTestFrames");
    auto *testInfo=w->findChild<QLabel*>("systemTestInfo");
    QVERIFY(testDump&&testInfo);
    auto *testFollow=w->findChild<QToolButton*>("systemTestViewFollow");
    auto *testCount=w->findChild<QToolButton*>("systemTestViewCount");
    auto *testMs=w->findChild<QToolButton*>("systemTestViewMs");
    auto *testHz=w->findChild<QToolButton*>("systemTestViewHz");
    auto *testLines=w->findChild<QPlainTextEdit*>("systemTestFollow");
    QVERIFY(testFollow&&testCount&&testMs&&testHz&&testLines);
    w->findChild<QAction*>("command33395")->trigger();
    QVERIFY(testInfo->text().contains("No verified decoder"));
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_COMPARE(testDump->rowCount(),1);
    QCOMPARE(testDump->item(0,0)->text(),QString("       1"));
    QCOMPARE(testDump->item(0,1)->text(),QString("124"));
    QVERIFY(testDump->item(0,1)->textAlignment()&Qt::AlignRight);
    QCOMPARE(testDump->columnCount(),4);
    QCOMPARE(testDump->horizontalHeaderItem(2)->text(),QString("avg [Hz]"));
    QVERIFY(testDump->item(0,2)->text().toDouble()>0.0);
    testMs->click();
    QCOMPARE(testDump->horizontalHeaderItem(2)->text(),QString("avg [ms]"));
    testFollow->click();
    QVERIFY(testLines->isVisible());QVERIFY(!testDump->isVisible());
    QVERIFY(testLines->toPlainText().contains("124 [3] 04 02 03"));
    testCount->click();
    testHz->click();
    QCOMPARE(testDump->item(0,3)->text(),QString("[3] 04 02 03"));
    w->findChild<QAction*>("command32956")->trigger();
    QVERIFY(testInfo->text().contains("documented CAN frame"));
    frame.can_id=CAN_EFF_FLAG|0x0c140000;
    frame.can_dlc=1;frame.data[0]=0xc0;
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_COMPARE(testDump->rowCount(),1);
    QCOMPARE(testDump->item(0,0)->text(),QString("       1"));
    QCOMPARE(testDump->item(0,1)->text(),QString("0C140000"));
    QCOMPARE(testDump->item(0,3)->text(),QString("[1] C0"));
    w->findChild<QAction*>("command32957")->trigger();
    frame.can_id=CAN_EFF_FLAG|0x03c30f0f;
    frame.can_dlc=8;std::fill_n(frame.data,8,0x87);
    QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
    QTRY_COMPARE(testDump->rowCount(),1);
    QCOMPARE(testDump->item(0,1)->text(),QString("03C30F0F"));
    QCOMPARE(testDump->item(0,0)->text(),QString("       1"));
    for(unsigned command:{32817u,33107u}){
      w->findChild<QAction*>("command"+QString::number(command))->trigger();
      frame.can_id=CAN_EFF_FLAG|0x02000100;
      frame.can_dlc=2;frame.data[0]=10;frame.data[1]=246;
      QCOMPARE(::write(fd,&frame,sizeof(frame)),ssize_t(sizeof(frame)));
      QTRY_COMPARE(testDump->rowCount(),1);
      QCOMPARE(testDump->item(0,0)->text(),QString("       1"));
      QCOMPARE(testDump->item(0,1)->text(),QString("02000100"));
      QCOMPARE(testDump->item(0,3)->text(),QString("[2] 0A F6"));
      QCOMPARE(testInfo->text(),QString("Raw joystick axes: X=10, Y=-10"));
    }
    ::close(fd);
    QTRY_COMPARE(w->model()->headings[2], QString("Latest payload"));
    QVERIFY(!w->monitoring());
    QCOMPARE(w->receivedFrames(), quint64(1));
  }
};
} // namespace rnet::qt
QTEST_MAIN(rnet::qt::WindowTest)
#include "qt_window_test.moc"
