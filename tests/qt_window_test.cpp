#include "i18n.hpp"
#include "window.hpp"
#include "hazard_blink_cycle.hpp"
#include "value_editor.hpp"
#include "app_settings.hpp"
#include "can_heartbeat.hpp"
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
    defaults.canInterface="rnetOffline";defaults.keepAwake=false;defaults.showAllParameters=false; // Never bind a physical interface in offline UI tests.
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
    QVERIFY_EXCEPTION_THROWN(languages().setLanguage("fr_FR"),
                             std::runtime_error);
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
    w->settings_.showAllParameters=true;w->showValues();w->setAccess(AccessLevel::Therapist);for(int i=0;i<w->filter()->rowCount();++i)QVERIFY(w->filter()->index(i,4).data().toString().startsWith("0"));
  }
  void coloredDongleStatusTracksRolesAndLanguage(){
    auto w=make();auto *label=w->findChild<QLabel*>("dongleStatus");QVERIFY(label);const QString colors[]{"#ffeb3b","#4caf50","#1976d2","#d32f2f"};
    for(int i=0;i<4;++i){w->setAccess(AccessLevel(i));QVERIFY(label->text().contains(QString::number(i)));QVERIFY(label->styleSheet().contains(colors[i]));}
    w->setAccess(AccessLevel::Dealer);languages().setLanguage("de_DE");QCoreApplication::sendPostedEvents(nullptr,QEvent::LanguageChange);QVERIFY(label->text().contains("Händler"));QVERIFY(label->styleSheet().contains(colors[1]));
    w->statusBar()->showMessage("Temporary status");QVERIFY(label->isVisible());languages().setLanguage("en_US");
  }
  void elevatedMenuRequiresWarningAndDoesNotPersist(){
    auto w=make();w->setAccess(AccessLevel::Dealer);
    QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Cancel)->click();});w->findChild<QAction*>("role3")->trigger();QCOMPARE(w->access_,AccessLevel::Dealer);QVERIFY(w->findChild<QAction*>("role1")->isChecked());
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
  void blinkArmsWithoutHeartbeatAndWaitsBeforeSending() {
    auto w = make();
    QVERIFY(!w->heartbeatReceiving());
    w->settings_.liveSignalIntervalSeconds = 23;
    w->settings_.liveSignalDurationSeconds = 7;
    auto *action = w->findChild<QAction*>("hazardBlinkAction");
    QVERIFY(action);
    QVERIFY(!action->isChecked());
    w->requestBlinkCycle();
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY(action->isChecked());
    QVERIFY(w->blinkCycle_->waiting());
    QVERIFY(!w->blinkCycle_->pending());
    QCOMPARE(w->blinkCycle_->intervalMs(), 23000);
    QCOMPARE(w->blinkCycle_->durationMs(), 7000);
    auto *indicator = w->findChild<QLabel*>("lightStatus");
    QVERIFY(indicator && indicator->isVisible());
    QVERIFY(indicator->text().isEmpty());
    QVERIFY(!indicator->pixmap().isNull());
    QCOMPARE(indicator->accessibleName(), QString("Lights: waiting for CAN"));
    w->close();
    QVERIFY(!w->blinkCycle_->active());
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
        dialog->reject();
      }
    });
    read->trigger();
    QVERIFY(dialogSeen);
    QVERIFY(w->process_ == nullptr);
    w->openDocument(fixture_);
    QVERIFY(read->isEnabled());
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
        QVERIFY_EXCEPTION_THROWN(encode_parameter_value(p,v,QString::number(value).toStdU16String(),w->catalog_),std::runtime_error);
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
    QVERIFY_EXCEPTION_THROWN(controller_write_plan(original,*w->document(),w->catalog_,AccessLevel::Therapist),std::runtime_error);
    auto bad=*w->document();bad.comments.push_back(u"unauthorized metadata");
    QVERIFY_EXCEPTION_THROWN(controller_write_plan(original,bad,w->catalog_,AccessLevel::Factory),std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(w->startControllerWrite("can0",temp_.path()+"/never",false),std::runtime_error);
    QVERIFY(!QFileInfo::exists(temp_.path()+"/never"));
    w->setAccess(AccessLevel::Therapist);
    QVERIFY_EXCEPTION_THROWN(w->startControllerWrite("can0",temp_.path()+"/never",true),std::runtime_error);
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
    QCOMPARE(w->model()->columnCount(), 5);
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
        auto index = w->filter()->mapToSource(w->filter()->index(row, 4));
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
    QCOMPARE(w->model()->headings[4], QString("Berechtigung"));
    QVERIFY(w->filter()->data(w->filter()->index(0, 4)).toString().contains("Händler"));
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
  void editorDialogThroughMouse() {
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
    QTimer::singleShot(50, w.get(), [] {
      auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if(dialog&&dialog->objectName()=="parameterValueDialog") {
        dialog->findChild<QLineEdit*>("numberValueEditor")->setText("86");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(w->isWindowModified());
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
    QTimer::singleShot(50, w.get(), [] {
      if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        auto *choice=dialog->findChild<QComboBox*>("choiceValueEditor");
        if(choice){choice->setCurrentIndex(choice->findData(QVariant::fromValue(qlonglong(1))));
          dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();}
      }
    });
    QTest::mouseClick(edit, Qt::LeftButton);
    QVERIFY(w->isWindowModified());
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
    QVERIFY_EXCEPTION_THROWN(w->saveAs(fixture_), std::runtime_error);
    QVERIFY(w->document()->write() == source);
    auto invalid = temp_.path() + "/invalid.R-net";
    QFile f(invalid);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("invalid");
    f.close();
    QVERIFY_EXCEPTION_THROWN(w->openDocument(invalid), std::runtime_error);
    QVERIFY(w->document()->write() == source);
    int index = firstSpeed(*w);
    QVERIFY_EXCEPTION_THROWN(w->applyEdit(index, "999999999999999999999999"),
                             std::runtime_error);
    QVERIFY(w->document()->write() == source);
    auto destination = temp_.path() + "/externally-modified.R-net";
    w->saveAs(destination);
    QFile external(destination);
    QVERIFY(external.open(QIODevice::WriteOnly));
    external.write("external change");
    external.close();
    QVERIFY_EXCEPTION_THROWN(w->save(), std::runtime_error);
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
    QVERIFY_EXCEPTION_THROWN(w->exportCsv(target), std::runtime_error);
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
    QVERIFY_EXCEPTION_THROWN(
        w->startControllerRead("rnetcfg0", destination, false),
        std::runtime_error);
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
    w->startMonitor(interface, 300);
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
    ::close(fd);
    QTRY_VERIFY_WITH_TIMEOUT(!w->monitoring(), 2000);
    QVERIFY(w->findChild<QAction *>("command32803")->isEnabled());
    QCOMPARE(w->receivedFrames(), quint64(1));
    QCOMPARE(w->model()->rowCount(), 1);
    QCOMPARE(w->model()->rows[0].value, QString("01 02 03"));
    languages().setLanguage("en_US");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
    QTRY_COMPARE(w->model()->headings[2], QString("Latest payload"));
    QVERIFY(!w->monitoring());
    QCOMPARE(w->receivedFrames(), quint64(1));
  }
};
} // namespace rnet::qt
QTEST_MAIN(rnet::qt::WindowTest)
#include "qt_window_test.moc"
