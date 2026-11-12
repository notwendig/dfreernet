#include "window.hpp"
#include "app_settings.hpp"
#include "runtime_resources.hpp"
#include "rnd_decoder.hpp"
#include "values.hpp"
#include <QtTest>
#include <QtWidgets>
using namespace rnet;
using namespace rnet::qt;
class StandaloneTest final : public QObject {
  Q_OBJECT
private slots:
  void catalogImportIsTransactionalAndDoesNotReplaceBundledData(){
    QTemporaryDir temp;const auto target=std::filesystem::path(temp.path().toStdString())/"imported.sqlite3";
    const auto before=bundled_catalog_bytes();
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,write_catalog_database(Bytes{1,2,3},target));QVERIFY(!std::filesystem::exists(target));
    write_catalog_database(before,target);
    auto actual=read_catalog_database(target),expected=bundled_catalog();QCOMPARE(actual.parameters.size(),expected.parameters.size());QCOMPARE(actual.storage_maps.size(),expected.storage_maps.size());QCOMPARE(actual.editors.size(),expected.editors.size());
    for(const auto &[key,p]:expected.parameters){const auto &q=actual.parameters.at(key);QVERIFY(p.metadata==q.metadata);QVERIFY(p.serialized_metadata==q.serialized_metadata);QVERIFY(p.extension==q.extension);QVERIFY(p.texts==q.texts);QVERIFY(p.option_texts==q.option_texts);QCOMPARE(p.text88,q.text88);QCOMPARE(p.text90,q.text90);QCOMPARE(p.signed_storage,q.signed_storage);}
    for(const auto &[key,m]:expected.storage_maps){const auto &n=actual.storage_maps.at(key);QCOMPARE(m.id,n.id);QCOMPARE(m.version,n.version);for(unsigned gi=0;gi<2;++gi){const auto &a=m.groups[gi],&b=n.groups[gi];QCOMPARE(a.instances,b.instances);QCOMPARE(a.stride,b.stride);QCOMPARE(a.last_section,b.last_section);QVERIFY(a.offsets==b.offsets);for(unsigned si=0;si<7;++si){QVERIFY(a.sections[si].fields==b.sections[si].fields);QCOMPARE(a.sections[si].items.size(),b.sections[si].items.size());for(unsigned i=0;i<a.sections[si].items.size();++i){QCOMPARE(a.sections[si].items[i].id,b.sections[si].items[i].id);QCOMPARE(a.sections[si].items[i].width,b.sections[si].items[i].width);}}}}
    const auto saved=read_file(target);QVERIFY_THROWS_EXCEPTION(std::runtime_error,write_catalog_database(before,target));QVERIFY(read_file(target)==saved);QVERIFY(bundled_catalog_bytes()==before);
    auto rnd=qEnvironmentVariable("DFREERNET_TEST_RND_PATH");if(!rnd.isEmpty()){const auto decoded=RndDecoder().decode(read_file(rnd.toStdString()));auto extra=target.parent_path()/"rnd.sqlite3";write_catalog_database(decoded,extra);QCOMPARE(read_catalog_database(extra).parameters.size(),expected.parameters.size());}
  }
  void embeddedAssetsAndDocumentEditing(){
    QTemporaryDir temp;QVERIFY(temp.isValid());
    QCoreApplication::setOrganizationName("dfreernetTests");QCoreApplication::setApplicationName("standalone");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    auto settings=AppSettings::defaults();settings.canInterface="rnetOffline";settings.keepAwake=false;settings.logDirectory=temp.path()+"/logs";settings.save();
    const Bytes encryptedPrefix={23,11,237,144,246,191,29,43,202,71,194,194,17,163,47,134,135,65,45,116,173,153,1,17,200,51,137,189,4,44,95,208,98,53,109,98,129,74,253,251,4,15,209,72,219,16,143,100,64,235,91,12,199,187,224,120,98,101,175,173,155,18,137,4};
    auto decodedPrefix=RndDecoder().decode(encryptedPrefix);QVERIFY(std::equal(decodedPrefix.begin(),decodedPrefix.end(),bundled_catalog_bytes().begin()));
    const auto catalog=bundled_catalog();QCOMPARE(catalog.parameters.size(),std::size_t(2154));QCOMPARE(catalog.storage_maps.size(),std::size_t(29));QCOMPARE(catalog.editors.size(),std::size_t(164));
    QCOMPARE(catalog.parameters.at(30).name(),catalog.parameters.at(2129).name());
    QVERIFY(catalog.parameters.at(30).text88!=catalog.parameters.at(2129).text88);
    QVERIFY(catalog.parameters.at(30).help()!=catalog.parameters.at(2129).help());
    QCOMPARE(QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(bundled_catalog_bytes().data()),int(bundled_catalog_bytes().size())),QCryptographicHash::Sha256).toHex(),QByteArray("30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc"));
    auto before=RnetDocument::read(bundled_default_document());QVERIFY(before.write()==bundled_default_document());
    const auto source=temp.path()+"/before.R-net";QFile f(source);QVERIFY(f.open(QIODevice::WriteOnly));auto bytes=before.write();QCOMPARE(f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()),qint64(bytes.size()));f.close();
    Window window(settings.logDirectory);window.show();window.openDocument(source);QVERIFY(window.valueCount()>1000);
    QVERIFY(window.windowTitle().contains(QStringLiteral(DFREERNET_VERSION)));
    auto *aboutAction=window.findChild<QAction*>("command57664");
    QVERIFY(aboutAction);
    auto *helpMenu=qobject_cast<QMenu*>(aboutAction->parent());
    QVERIFY(helpMenu);
    QCOMPARE(window.menuBar()->actions().last(),helpMenu->menuAction());
    QString aboutHeading,aboutContact;
    QTimer::singleShot(0,&window,[&]{
      auto *dialog=window.findChild<QDialog*>("aboutDialog");
      if(!dialog)return;
      auto *heading=dialog->findChild<QLabel*>("aboutHeading");
      auto *contact=dialog->findChild<QLabel*>("aboutContact");
      if(heading)aboutHeading=heading->text();
      if(contact){aboutContact=contact->text();QVERIFY(contact->openExternalLinks());}
      dialog->accept();
    });
    aboutAction->trigger();
    QVERIFY(aboutHeading.contains(QStringLiteral(DFREERNET_VERSION)));
    QVERIFY(aboutContact.contains(QString::fromUtf8("Jürgen Willi Sievers")));
    QVERIFY(aboutContact.contains("mailto:JSievers@NadiSoft.de"));
    QVERIFY(aboutContact.contains("https://github.com/notwendig/dfreernet"));
    auto values=read_values(before,catalog);int index=-1;for(std::size_t i=0;i<values.size();++i)if(values[i].parameter==68){index=int(i);break;}QVERIFY(index>=0);
    auto *table=window.findChild<QTableView*>("parameters");
    auto *saveColumns=window.findChild<QAction*>("saveColumnLayout");
    auto *loadColumns=window.findChild<QAction*>("loadColumnLayout");
    auto *resetColumns=window.findChild<QAction*>("resetColumnLayout");
    auto *helpAction=window.findChild<QAction*>("parameterHelp");
    QVERIFY(table&&helpAction&&saveColumns&&loadColumns&&resetColumns);
    QVERIFY(saveColumns->isEnabled());
    QVERIFY(!loadColumns->isEnabled());
    table->setColumnWidth(0,210);
    table->horizontalHeader()->moveSection(0,1);
    saveColumns->trigger();
    QVERIFY(loadColumns->isEnabled());
    table->setColumnWidth(0,145);
    table->horizontalHeader()->moveSection(1,0);
    loadColumns->trigger();
    QCOMPARE(table->columnWidth(0),210);
    QCOMPARE(table->horizontalHeader()->visualIndex(0),1);
    QCOMPARE(helpAction->shortcut(),QKeySequence(Qt::Key_F1));
    int helpRow=-1;
    for(int row=0;row<window.model()->rowCount();++row)
      if(window.model()->rows[std::size_t(row)].valueIndex==index){helpRow=row;break;}
    QVERIFY(helpRow>=0);
    table->setCurrentIndex(window.filter()->mapFromSource(window.model()->index(helpRow,2)));
    QVERIFY(helpAction->isEnabled());
    QString shownHelp,shownContext;
    QTimer::singleShot(0,&window,[&]{
      auto *dialog=window.findChild<QDialog*>("parameterHelpDialog");
      if(!dialog)return;
      shownHelp=dialog->findChild<QPlainTextEdit*>("parameterHelpText")->toPlainText();
      shownContext=dialog->findChild<QLabel*>("parameterHelpContext")->text();
      dialog->accept();
    });
    table->setFocus();
    QTest::keyClick(table,Qt::Key_F1);
    QCOMPARE(shownHelp,fromU16(catalog.parameters.at(68).help()).trimmed());
    QVERIFY(shownContext.contains("Stored raw value:"));
    QVERIFY(shownContext.contains(QString::number(values[index].number)));
    auto *profileAction=window.findChild<QAction*>("profileTableView");
    auto *listAction=window.findChild<QAction*>("hierarchicalListView");
    auto *moduleAction=window.findChild<QAction*>("moduleGroupView");
    auto *fixedAccess=window.findChild<QTableView*>("fixedAccessColumn");
    QVERIFY(profileAction&&listAction&&moduleAction&&fixedAccess);
    auto *profileButton=window.findChild<QToolButton*>("profileTableButton");
    auto *listButton=window.findChild<QToolButton*>("hierarchicalListButton");
    QVERIFY(profileButton&&listButton);
    QVERIFY(profileButton->isVisible()&&listButton->isVisible());
    QCOMPARE(profileButton->defaultAction(),profileAction);
    profileAction->trigger();
    QVERIFY(profileButton->isChecked());
    QVERIFY(!loadColumns->isEnabled());
    table->setColumnWidth(0,175);
    saveColumns->trigger();
    table->setColumnWidth(0,181);
    moduleAction->trigger();
    table->setColumnWidth(0,202);
    profileAction->trigger();
    QCOMPARE(table->columnWidth(0),181);
    moduleAction->trigger();
    QCOMPARE(table->columnWidth(0),202);
    profileAction->trigger();
    QTest::qWait(300);
    QVERIFY(QSettings().contains("ui/columns/v1/modules"));
    auto *tree=window.findChild<QTreeWidget*>("groups");
    QCOMPARE(tree->topLevelItemCount(),9);
    QCOMPARE(tree->topLevelItem(0)->text(0),QString("Common"));
    QCOMPARE(tree->topLevelItem(8)->text(0),QString("Profile 8"));
    QCOMPARE(window.filter()->group,QString("common"));
    QCOMPARE(window.model()->headings.at(3),QString::fromUtf8("Unit · Scale · Min–Max"));
    QCOMPARE(window.model()->headerData(3,Qt::Horizontal).toString(),QString::fromUtf8("Unit · Scale\nMin–Max"));
    QCOMPARE(window.model()->headings.at(5),QString("Module / group"));
    QCOMPARE(window.model()->headerData(5,Qt::Horizontal).toString(),QString("Module /\ngroup"));
    QCOMPARE(window.model()->headings.at(6),QString("Access level"));
    QVERIFY(table->isColumnHidden(6));
    QVERIFY(fixedAccess->isVisible());
    QVERIFY(!fixedAccess->isColumnHidden(6));
    QCOMPARE(fixedAccess->model(),table->model());
    bool found=false;
    for(int row=0;row<window.model()->rowCount();++row)
      {
        const auto cell=window.model()->index(row,2);
        if(window.model()->rows[std::size_t(row)].valueIndex!=index)continue;
        QCOMPARE(cell.data().toString(),QString::number(values[index].number));
        auto expectedModule=fromU16(catalog.parameters.at(values[index].parameter).text88);
        expectedModule.replace('~',QStringLiteral(" / "));
        QCOMPARE(window.model()->index(row,5).data().toString(),expectedModule);
        QCOMPARE(window.model()->index(row,3).data().toString(),QString::fromUtf8("— · 1:1 · 0–1"));
        found=true;
      }
    QVERIFY(found);
    window.applyEdit(index,values[index].number?"0":"1");
    found=false;
    for(int row=0;row<window.model()->rowCount();++row)
      {
        const auto cell=window.model()->index(row,2);
        if(window.model()->rows[std::size_t(row)].valueIndex!=index)continue;
        QCOMPARE(cell.data(Qt::ForegroundRole).value<QColor>(),QColor(Qt::red));
        QVERIFY(cell.data(Qt::FontRole).value<QFont>().bold());
        found=true;
      }
    QVERIFY(found);
    tree->setCurrentItem(tree->topLevelItem(1));
    QCOMPARE(window.filter()->group,QString("profile1"));
    for(int row=0;row<window.filter()->rowCount();++row)
      QCOMPARE(window.model()->rows[std::size_t(window.filter()->mapToSource(window.filter()->index(row,0)).row())].group,QString("profile1"));
    moduleAction->trigger();
    QVERIFY(moduleAction->isChecked());
    QVERIFY(QSettings().value("ui/moduleGroupView").toBool());
    QCOMPARE(window.model()->headings.at(3),QString::fromUtf8("Unit · Scale · Min–Max"));
    QVERIFY(tree->topLevelItem(0)->childCount()>0);
    auto *moduleItem=tree->topLevelItem(0)->child(0);
    tree->setCurrentItem(moduleItem);
    QCOMPARE(window.filter()->group,moduleItem->data(0,Qt::UserRole).toString());
    QVERIFY(window.filter()->rowCount()>0);
    for(int row=0;row<window.filter()->rowCount();++row)
      QCOMPARE(window.model()->rows[std::size_t(window.filter()->mapToSource(window.filter()->index(row,0)).row())].group,window.filter()->group);
    QVERIFY(table->isColumnHidden(window.model()->columnCount()-1));
    QVERIFY(!fixedAccess->isColumnHidden(window.model()->columnCount()-1));
    listAction->trigger();
    QVERIFY(!QSettings().value("ui/moduleGroupView").toBool());
    QCOMPARE(table->columnWidth(0),210);
    QCOMPARE(table->horizontalHeader()->visualIndex(0),1);
    resetColumns->trigger();
    QCOMPARE(table->columnWidth(0),130);
    QCOMPARE(table->horizontalHeader()->visualIndex(0),0);
    QVERIFY(!loadColumns->isEnabled());
    QVERIFY(window.model()->rowCount()>=window.valueCount());
    auto target=temp.path()+"/after.R-net";window.saveAs(target);auto after=RnetDocument::read(read_file(target.toStdString()));QVERIFY(!controller_write_plan(before,after,catalog,AccessLevel::OEM).empty());window.openDocument(target);QVERIFY(window.document()->write()==after.write());
    window.showResources();QCOMPARE(window.model()->rowCount(),1454);
    table->setColumnWidth(0,205);
    saveColumns->trigger();
    table->setColumnWidth(0,140);
    loadColumns->trigger();
    QCOMPARE(table->columnWidth(0),205);
    resetColumns->trigger();
    QCOMPARE(table->columnWidth(0),130);
    QVERIFY(window.findChild<QAction*>("command40040"));
    if(auto path=qEnvironmentVariable("DFREERNET_SMOKE_SCREENSHOT");!path.isEmpty()){
      QVERIFY(window.grab().save(path));
      auto *action=window.findChild<QAction*>("command40002");QVERIFY(action);auto *menu=qobject_cast<QMenu*>(action->parent());QVERIFY(menu);menu->popup(window.mapToGlobal(QPoint(10,25)));QTest::qWait(50);QVERIFY(menu->grab().save(path+"-rnd-menu.png"));menu->hide();
    }
    window.showValues();
    table->setColumnWidth(1,555);
    window.close();
    QVERIFY(QSettings().contains("ui/columns/v1/hierarchy"));
    Window restarted(settings.logDirectory);
    restarted.show();
    QCOMPARE(restarted.windowTitle(),QStringLiteral("dfreernet ")+QStringLiteral(DFREERNET_VERSION));
    QCOMPARE(restarted.findChild<QTableView*>("parameters")->columnWidth(1),555);
    restarted.findChild<QAction*>("profileTableView")->trigger();
    QCOMPARE(restarted.findChild<QTableView*>("parameters")->columnWidth(0),181);
    restarted.placeOnSecondary();
    QTRY_COMPARE(restarted.findChild<QTableView*>("parameters")->columnWidth(0),181);
    restarted.close();
  }
};
QTEST_MAIN(StandaloneTest)
#include "standalone_test.moc"
