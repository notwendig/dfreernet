#include "ir_document.hpp"
#include "editor_window.hpp"
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>
using namespace iredit;
namespace {
QString example() {
    // Synthetic 32-bit pulse-distance signal; not a bundled manufacturer's code.
    QStringList w = {"0000", "006F", "0000", "0022", "0151", "00A7"};
    for (int i = 0; i < 32; ++i) w << "0015" << (i%2 ? "003F" : "0015");
    return w.join(' ') + " 0016 05D5";
}
Document sample() {
    Document d; int device = d.addFolder("Device"); int cmd = d.addCommand("Input 1", device); d.setCode(cmd, 0, Pronto::parse(example()).toRnet()); d.items[cmd].flags |= 0x08;
    int other = d.addFolder("Second"); int nested = d.addFolder("Folder", other); int alt = d.addCommand("Other", nested);
    d.setCode(alt, 1, QByteArray::fromHex("01020304")); d.shortcuts[0] = char(alt); return d;
}
void same(const Document &a, const Document &b) {
    QCOMPARE(a.items.size(), b.items.size()); QCOMPARE(a.shortcuts, b.shortcuts);
    for (int i=0; i<a.items.size(); ++i) {
        QCOMPARE(a.items[i].name,b.items[i].name); QCOMPARE(a.items[i].flags,b.items[i].flags); QCOMPARE(a.items[i].parent,b.items[i].parent); QCOMPARE(a.items[i].textIndex,b.items[i].textIndex);
        for (int slot=0;slot<2;++slot) QCOMPARE(a.items[i].codes[slot],b.items[i].codes[slot]);
    }
}
}
class IrEditorTest : public QObject {
    Q_OBJECT
private slots:
    void pulseEncoding() {
        auto p=Pronto::parse(example()); QCOMPARE(p.pairs(),34); QVERIFY(p.frequency()>37000 && p.frequency()<38000);
        auto native=p.toRnet(); QCOMPARE(native.size(),76); QCOMPARE(native.left(12).toHex(),QByteArray("026b00205101a7001600d505"));
        QCOMPARE(Pronto::parse(Pronto::fromRnet(native)).toRnet(),native);
        auto both=p; both.words[2]=1; both.words[3]=33; QVERIFY_EXCEPTION_THROWN(both.toRnet(),std::runtime_error);
        auto intro=p; intro.words[2]=34;intro.words[3]=0;QCOMPARE(intro.toRnet(),native);
        QVERIFY_EXCEPTION_THROWN(Pronto::parse("0000 0000 0000 0000"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Pronto::parse("0000 006F 0000 0001 0015"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Pronto::parse("5000 006F 0000 0002 0015 0015 0015 0015"),std::runtime_error);
        auto longPulse=p;longPulse.words[6]=256;QVERIFY_EXCEPTION_THROWN(longPulse.toRnet(),std::runtime_error);
        auto zero=p;zero.words[4]=0;QVERIFY_EXCEPTION_THROWN(Pronto::parse(zero.text()),std::runtime_error);
        QVERIFY(Pronto::fromRnet(QByteArray::fromHex("01020304")).isEmpty());
    }
    void documentRoundTrip() {
        auto d=sample();same(d,Document::fromRnir3(d.rnir3()));same(d,Document::fromJson(d.json()));
        d.items[0].name="Gerät – 日本語";same(d,Document::fromRnir3(d.rnir3()));
        Document empty;same(empty,Document::fromRnir3(empty.rnir3()));
        auto large=sample();large.items[1].codes[0]=QByteArray(65536,char(0xa5));same(large,Document::fromRnir3(large.rnir3()));
    }
    void invalidFiles() {
        const auto bytes=sample().rnir3();
        for(int size: {0,1,16,40,int(bytes.size()-1)}) QVERIFY_EXCEPTION_THROWN(Document::fromRnir3(bytes.left(size)),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Document::fromRnir3(bytes+'x'),std::runtime_error);
        auto bad=sample();bad.items[1].parent=2;QVERIFY_EXCEPTION_THROWN(bad.validate(),std::runtime_error);
        bad=sample();bad.items[1].parent=255;QVERIFY_EXCEPTION_THROWN(bad.validate(),std::runtime_error);
        bad=sample();bad.shortcuts[0]=char(250);QVERIFY_EXCEPTION_THROWN(bad.validate(),std::runtime_error);
        auto json=QJsonDocument::fromJson(sample().json()).object();json["shortcuts"]="xx";
        QVERIFY_EXCEPTION_THROWN(Document::fromJson(QJsonDocument(json).toJson()),std::runtime_error);
        // Exercise the bounds checks throughout a complete stream, not only its header.
        for (int i=0;i<bytes.size();i+=17) { auto changed=bytes;changed[i]=char(255);try{auto d=Document::fromRnir3(changed);d.validate();}catch(const std::runtime_error &){} }
    }
    void deletionPreservesReferences() {
        auto d=sample(); auto untouched=d.items.last(); d.remove(0);
        QCOMPARE(d.items.size(),3);QCOMPARE(d.items[1].parent,quint8(1));QCOMPARE(d.items[2].parent,quint8(2));QCOMPARE(quint8(d.shortcuts[0]),quint8(2));QCOMPARE(d.items.last().codes[1],untouched.codes[1]);
        d.remove(1);QCOMPARE(d.items.size(),1);QCOMPARE(quint8(d.shortcuts[0]),quint8(255));
        auto flags=sample();flags.setCode(1,0,{});QVERIFY(!(flags.items[1].flags&0x20));
    }
    void scrolledCommandRemainsVisible() {
        QTemporaryDir temp;QVERIFY(temp.isValid());Document d;
        auto first=d.addFolder("Existing device");for(int i=0;i<85;++i)d.addCommand(QString("Command %1").arg(i),first);
        auto device=d.addFolder("ezcoo");d.addCommand("1",device);d.addCommand("2",device);
        auto file=temp.filePath("scrolled.RnIR3");d.save(file);
        EditorWindow window;window.openFile(file);window.show();QTest::qWait(30);
        auto *tree=window.findChild<QTreeWidget *>("irMenuTree");auto *name=window.findChild<QLineEdit *>("irEntryName");auto *apply=window.findChild<QPushButton *>("irApplyEntry");QVERIFY(tree&&name&&apply);
        auto matches=tree->findItems("ezcoo",Qt::MatchExactly|Qt::MatchRecursive);QCOMPARE(matches.size(),1);
        auto *one=matches[0]->child(0);tree->scrollToItem(one);QTest::qWait(20);
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(one).center());
        QCOMPARE(name->text(),QString("1"));
        QVERIFY2(tree->viewport()->rect().intersects(tree->visualItemRect(tree->currentItem())),"Selected command moved offscreen after clicking it");
        name->setText("1 edited");QTest::mouseClick(apply,Qt::LeftButton);
        QCOMPARE(tree->currentItem()->text(0),QString("1 edited"));
        QVERIFY2(tree->viewport()->rect().intersects(tree->visualItemRect(tree->currentItem())),"Selected command moved offscreen after applying an edit");
        auto *save=window.findChild<QAction *>("irSaveMenu");QVERIFY(save);save->trigger();window.close();
    }
    void localSaveAndUiEdit() {
        QTemporaryDir temp;QVERIFY(temp.isValid());auto input=temp.filePath("before.RnIR3");auto after=temp.filePath("after.RnIR3");sample().save(input);
        QFile original(input);QVERIFY(original.open(QIODevice::ReadOnly));const auto saved=original.readAll();original.close();
        auto invalid=sample();invalid.items[1].parent=255;QVERIFY_EXCEPTION_THROWN(invalid.save(input),std::runtime_error);
        QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),saved);original.close();
        EditorWindow window;window.openFile(input);window.show();
        auto *tree=window.findChild<QTreeWidget *>("irMenuTree");auto *name=window.findChild<QLineEdit *>("irEntryName");auto *pronto=window.findChild<QPlainTextEdit *>("irProntoInput");auto *apply=window.findChild<QPushButton *>("irApplyEntry");
        QVERIFY(tree && name && pronto && apply);QCOMPARE(tree->topLevelItemCount(),2);QCOMPARE(name->text(),QString("Input 1"));
        auto *collapse=window.findChild<QAction *>("irCollapseAll");auto *expand=window.findChild<QAction *>("irExpandAll");QVERIFY(collapse && expand);
        const auto draft=pronto->toPlainText();
        collapse->trigger();QVERIFY(!tree->topLevelItem(0)->isExpanded());QVERIFY(!tree->topLevelItem(1)->child(0)->isExpanded());
        QCOMPARE(pronto->toPlainText(),draft);QCOMPARE(name->text(),QString("Input 1"));
        expand->trigger();QVERIFY(tree->topLevelItem(0)->isExpanded());QVERIFY(tree->topLevelItem(1)->child(0)->isExpanded());
        name->setText("Renamed input");QVERIFY(apply->isEnabled());QTest::mouseClick(apply,Qt::LeftButton);QCOMPARE(tree->topLevelItem(0)->child(0)->text(0),QString("Renamed input"));
        auto *save=window.findChild<QAction *>("irSaveMenu");auto *undo=window.findChild<QAction *>("irUndo");auto *redo=window.findChild<QAction *>("irRedo");QVERIFY(save && undo && redo);
        save->trigger();auto renamed=sample();renamed.items[1].name="Renamed input";same(renamed,Document::load(input));
        // A real paste/apply/save edits only the selected signal and retains the unknown alternate signal elsewhere.
        auto replacement=Pronto::parse(example());replacement.words[7]=63;
        pronto->setPlainText(replacement.text());QVERIFY(apply->isEnabled());QTest::mouseClick(apply,Qt::LeftButton);save->trigger();
        auto changed=renamed;changed.setCode(1,0,replacement.toRnet());same(changed,Document::load(input));
        undo->trigger();save->trigger();same(renamed,Document::load(input));
        redo->trigger();save->trigger();same(changed,Document::load(input));
        // The independent file remains unaffected by edits to the open menu.
        sample().save(after);same(sample(),Document::load(after));
        window.close();
    }
};
QTEST_MAIN(IrEditorTest)
#include "ir_editor_test.moc"
