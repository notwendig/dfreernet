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
    QVERIFY_EXCEPTION_THROWN(write_catalog_database(Bytes{1,2,3},target),std::runtime_error);QVERIFY(!std::filesystem::exists(target));
    write_catalog_database(before,target);
    auto actual=read_catalog_database(target),expected=bundled_catalog();QCOMPARE(actual.parameters.size(),expected.parameters.size());QCOMPARE(actual.storage_maps.size(),expected.storage_maps.size());QCOMPARE(actual.editors.size(),expected.editors.size());
    for(const auto &[key,p]:expected.parameters){const auto &q=actual.parameters.at(key);QVERIFY(p.metadata==q.metadata);QVERIFY(p.serialized_metadata==q.serialized_metadata);QVERIFY(p.extension==q.extension);QVERIFY(p.texts==q.texts);QVERIFY(p.option_texts==q.option_texts);QCOMPARE(p.text88,q.text88);QCOMPARE(p.text90,q.text90);QCOMPARE(p.signed_storage,q.signed_storage);}
    for(const auto &[key,m]:expected.storage_maps){const auto &n=actual.storage_maps.at(key);QCOMPARE(m.id,n.id);QCOMPARE(m.version,n.version);for(unsigned gi=0;gi<2;++gi){const auto &a=m.groups[gi],&b=n.groups[gi];QCOMPARE(a.instances,b.instances);QCOMPARE(a.stride,b.stride);QCOMPARE(a.last_section,b.last_section);QVERIFY(a.offsets==b.offsets);for(unsigned si=0;si<7;++si){QVERIFY(a.sections[si].fields==b.sections[si].fields);QCOMPARE(a.sections[si].items.size(),b.sections[si].items.size());for(unsigned i=0;i<a.sections[si].items.size();++i){QCOMPARE(a.sections[si].items[i].id,b.sections[si].items[i].id);QCOMPARE(a.sections[si].items[i].width,b.sections[si].items[i].width);}}}}
    const auto saved=read_file(target);QVERIFY_EXCEPTION_THROWN(write_catalog_database(before,target),std::runtime_error);QVERIFY(read_file(target)==saved);QVERIFY(bundled_catalog_bytes()==before);
    auto rnd=qEnvironmentVariable("DFREERNET_TEST_RND_PATH");if(!rnd.isEmpty()){const auto decoded=RndDecoder().decode(read_file(rnd.toStdString()));auto extra=target.parent_path()/"rnd.sqlite3";write_catalog_database(decoded,extra);QCOMPARE(read_catalog_database(extra).parameters.size(),expected.parameters.size());}
  }
  void embeddedAssetsAndDocumentEditingWithoutInstallation(){
    QTemporaryDir temp;QVERIFY(temp.isValid());
    QCoreApplication::setOrganizationName("dfreernetTests");QCoreApplication::setApplicationName("standalone");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    auto settings=AppSettings::defaults();settings.canInterface="rnetOffline";settings.keepAwake=false;settings.logDirectory=temp.path()+"/logs";settings.save();
    const Bytes encryptedPrefix={23,11,237,144,246,191,29,43,202,71,194,194,17,163,47,134,135,65,45,116,173,153,1,17,200,51,137,189,4,44,95,208,98,53,109,98,129,74,253,251,4,15,209,72,219,16,143,100,64,235,91,12,199,187,224,120,98,101,175,173,155,18,137,4};
    auto decodedPrefix=RndDecoder().decode(encryptedPrefix);QVERIFY(std::equal(decodedPrefix.begin(),decodedPrefix.end(),bundled_catalog_bytes().begin()));
    const auto catalog=bundled_catalog();QCOMPARE(catalog.parameters.size(),std::size_t(2154));QCOMPARE(catalog.storage_maps.size(),std::size_t(29));QCOMPARE(catalog.editors.size(),std::size_t(164));
    QCOMPARE(QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(bundled_catalog_bytes().data()),int(bundled_catalog_bytes().size())),QCryptographicHash::Sha256).toHex(),QByteArray("30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc"));
    auto before=RnetDocument::read(bundled_default_document());QVERIFY(before.write()==bundled_default_document());
    const auto source=temp.path()+"/before.R-net";QFile f(source);QVERIFY(f.open(QIODevice::WriteOnly));auto bytes=before.write();QCOMPARE(f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()),qint64(bytes.size()));f.close();
    Window window(settings.logDirectory);window.show();window.openDocument(source);QVERIFY(window.valueCount()>1000);
    auto values=read_values(before,catalog);int index=-1;for(std::size_t i=0;i<values.size();++i)if(values[i].parameter==68){index=int(i);break;}QVERIFY(index>=0);window.applyEdit(index,values[index].number?"0":"1");
    auto target=temp.path()+"/after.R-net";window.saveAs(target);auto after=RnetDocument::read(read_file(target.toStdString()));QVERIFY(!controller_write_plan(before,after,catalog,AccessLevel::OEM).empty());window.openDocument(target);QVERIFY(window.document()->write()==after.write());
    window.showResources();QCOMPARE(window.model()->rowCount(),1454);
    QVERIFY(window.findChild<QAction*>("command40040"));
    QVERIFY(!window.findChild<QLineEdit*>("settingsInstallationDirectory"));
    if(auto path=qEnvironmentVariable("DFREERNET_SMOKE_SCREENSHOT");!path.isEmpty()){
      QVERIFY(window.grab().save(path));
      auto *action=window.findChild<QAction*>("command40002");QVERIFY(action);auto *menu=qobject_cast<QMenu*>(action->parent());QVERIFY(menu);menu->popup(window.mapToGlobal(QPoint(10,25)));QTest::qWait(50);QVERIFY(menu->grab().save(path+"-rnd-menu.png"));menu->hide();
    }
    window.close();
  }
};
QTEST_MAIN(StandaloneTest)
#include "standalone_test.moc"
