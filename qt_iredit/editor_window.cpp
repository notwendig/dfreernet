#include "editor_window.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QTreeWidgetItemIterator>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QSet>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
namespace iredit {
namespace {
QString text(const char *s) { return QCoreApplication::translate("RNet", s); }
class Edit final : public QUndoCommand {
    std::function<void(const Document &, int)> apply_;
    Document before_, after_;
    int old_, next_;
public:
    Edit(std::function<void(const Document &, int)> apply, Document before, Document after, int oldSelection, int newSelection, const QString &label)
        : QUndoCommand(label), apply_(std::move(apply)), before_(std::move(before)), after_(std::move(after)), old_(oldSelection), next_(newSelection) {}
    void undo() override { apply_(before_, old_); }
    void redo() override { apply_(after_, next_); }
};
}
void EditorWindow::bind(QObject *object, const char *source) { labels_.append({object, source}); object->setProperty("text", text(source)); }
EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent) {
    resize(1180, 800); setMinimumSize(860, 620);
    undo_ = new QUndoStack(this); undo_->setUndoLimit(100);
    auto *fileMenu = menuBar()->addMenu(QString()); bind(fileMenu->menuAction(), QT_TRANSLATE_NOOP("RNet", "&File"));
    auto action = [&](QMenu *menu, const char *source, const QKeySequence &shortcut, auto callback) {
        auto *a = menu->addAction(QString()); bind(a, source); a->setShortcut(shortcut); connect(a, &QAction::triggered, this, callback); return a;
    };
    action(fileMenu, QT_TRANSLATE_NOOP("RNet", "&New IR menu"), QKeySequence::New, [this] {
        if (!maybeSave()) return;
        document_ = {}; file_.clear(); undo_->clear(); pending_ = false; refresh(-1);
    });
    action(fileMenu, QT_TRANSLATE_NOOP("RNet", "&Open IR menu…"), QKeySequence::Open, [this] {
        if (!maybeSave()) return;
        auto f = QFileDialog::getOpenFileName(this, text("Open IR menu"), QFileInfo(file_).absolutePath(), text("IR menus (*.RnIR3 *.rnir3 *.json);;All files (*)"));
        if (f.isEmpty()) return;
        try { openFile(f); } catch (const std::exception &e) { error(e); }
    });
    auto *saveAction = action(fileMenu, QT_TRANSLATE_NOOP("RNet", "&Save IR menu"), QKeySequence::Save, [this] { save(false); });
    saveAction->setObjectName("irSaveMenu");
    action(fileMenu, QT_TRANSLATE_NOOP("RNet", "Save IR menu &as…"), QKeySequence::SaveAs, [this] { save(true); });
    fileMenu->addSeparator(); action(fileMenu, QT_TRANSLATE_NOOP("RNet", "&Quit"), QKeySequence::Quit, [this] { close(); });
    auto *editMenu = menuBar()->addMenu(QString()); bind(editMenu->menuAction(), QT_TRANSLATE_NOOP("RNet", "&Edit"));
    auto *undoAction = editMenu->addAction(QString()); bind(undoAction, QT_TRANSLATE_NOOP("RNet", "&Undo IR edit")); undoAction->setShortcut(QKeySequence::Undo); undoAction->setObjectName("irUndo");
    auto *redoAction = editMenu->addAction(QString()); bind(redoAction, QT_TRANSLATE_NOOP("RNet", "&Redo IR edit")); redoAction->setShortcut(QKeySequence::Redo); redoAction->setObjectName("irRedo");
    connect(undoAction, &QAction::triggered, this, [this] { if (applyEditor()) undo_->undo(); });
    connect(redoAction, &QAction::triggered, this, [this] { if (applyEditor()) undo_->redo(); });
    connect(undo_, &QUndoStack::canUndoChanged, undoAction, &QAction::setEnabled); undoAction->setEnabled(false);
    connect(undo_, &QUndoStack::canRedoChanged, redoAction, &QAction::setEnabled); redoAction->setEnabled(false);
    auto *language = menuBar()->addMenu(QString()); bind(language->menuAction(), QT_TRANSLATE_NOOP("RNet", "Language"));
    for (const auto &p : {qMakePair(QString("English"), QString("en_US")), qMakePair(QString("Deutsch"), QString("de_DE"))}) {
        auto *a = language->addAction(p.first); connect(a, &QAction::triggered, this, [this, code=p.second] { if (changeLanguage) changeLanguage(code); });
    }
    auto *tools = addToolBar("IR"); tools->setMovable(false);
    auto *device = tools->addAction(QString()); bind(device, QT_TRANSLATE_NOOP("RNet", "Add device")); device->setObjectName("irAddDevice"); connect(device, &QAction::triggered, this, &EditorWindow::addDevice);
    auto *command = tools->addAction(QString()); bind(command, QT_TRANSLATE_NOOP("RNet", "Add command")); command->setObjectName("irAddCommand"); connect(command, &QAction::triggered, this, &EditorWindow::addCommand);
    auto *remove = tools->addAction(QString()); bind(remove, QT_TRANSLATE_NOOP("RNet", "Remove entry")); connect(remove, &QAction::triggered, this, &EditorWindow::removeEntry);
    tools->addSeparator();
    auto *collapse = tools->addAction(QString());
    bind(collapse, QT_TRANSLATE_NOOP("RNet", "Collapse all"));
    collapse->setObjectName("irCollapseAll");
    connect(collapse, &QAction::triggered, this, [this] { expandAll_ = false; tree_->collapseAll(); });
    auto *expand = tools->addAction(QString());
    bind(expand, QT_TRANSLATE_NOOP("RNet", "Expand all"));
    expand->setObjectName("irExpandAll");
    connect(expand, &QAction::triggered, this, [this] { expandAll_ = true; tree_->expandAll(); });
    auto *central = new QWidget; auto *layout = new QVBoxLayout(central); layout->setContentsMargins(14, 12, 14, 12); layout->setSpacing(12); setCentralWidget(central);
    auto *note = new QLabel; bind(note, QT_TRANSLATE_NOOP("RNet", "Offline IR editor · R-net signal conversion is experimental. Exported signals require verification on the target device."));
    note->setWordWrap(true); note->setStyleSheet("QLabel { background: #fff0c2; color: #513c0a; padding: 12px; border: 1px solid #d1b868; border-radius: 5px; }"); layout->addWidget(note);
    auto *split = new QSplitter; layout->addWidget(split, 1);
    tree_ = new QTreeWidget; tree_->setObjectName("irMenuTree"); tree_->setMinimumWidth(240); tree_->setColumnCount(2); tree_->setColumnWidth(0, 240); tree_->setAlternatingRowColors(true); split->addWidget(tree_);
    auto *panel = new QWidget; auto *right = new QVBoxLayout(panel); right->setContentsMargins(16, 0, 0, 0); right->setSpacing(10); split->addWidget(panel); split->setSizes({360, 800});
    auto *form = new QFormLayout; right->addLayout(form);
    auto *nameLabel = new QLabel; bind(nameLabel, QT_TRANSLATE_NOOP("RNet", "Entry name")); name_ = new QLineEdit; name_->setObjectName("irEntryName"); form->addRow(nameLabel, name_);
    auto *slotLabel = new QLabel; bind(slotLabel, QT_TRANSLATE_NOOP("RNet", "Signal slot")); slotBox_ = new QComboBox; slotBox_->setObjectName("irSignalSlot"); slotBox_->addItems({"A", "B"}); form->addRow(slotLabel, slotBox_);
    auto *prontoLabel = new QLabel; bind(prontoLabel, QT_TRANSLATE_NOOP("RNet", "Pronto Hex · paste a complete code beginning with 0000")); prontoLabel->setWordWrap(true); right->addWidget(prontoLabel);
    pronto_ = new QPlainTextEdit; pronto_->setObjectName("irProntoInput"); pronto_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); pronto_->setMinimumHeight(160); right->addWidget(pronto_, 3);
    codeStatus_ = new QLabel; codeStatus_->setObjectName("irCodeStatus"); codeStatus_->setWordWrap(true); right->addWidget(codeStatus_);
    auto *buttons = new QHBoxLayout; right->addLayout(buttons);
    import_ = new QPushButton; bind(import_, QT_TRANSLATE_NOOP("RNet", "Import Pronto file…")); buttons->addWidget(import_);
    copy_ = new QPushButton; bind(copy_, QT_TRANSLATE_NOOP("RNet", "Copy Pronto")); buttons->addWidget(copy_);
    clear_ = new QPushButton; bind(clear_, QT_TRANSLATE_NOOP("RNet", "Clear signal")); buttons->addWidget(clear_); buttons->addStretch();
    apply_ = new QPushButton; bind(apply_, QT_TRANSLATE_NOOP("RNet", "Apply entry")); apply_->setObjectName("irApplyEntry"); apply_->setDefault(true); buttons->addWidget(apply_);
    auto *rawLabel = new QLabel; bind(rawLabel, QT_TRANSLATE_NOOP("RNet", "Stored R-net bytes · preserved when no replacement code is entered")); rawLabel->setWordWrap(true); right->addWidget(rawLabel);
    raw_ = new QPlainTextEdit; raw_->setReadOnly(true); raw_->setObjectName("irStoredBytes"); raw_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); raw_->setMaximumHeight(120); right->addWidget(raw_, 1);
    summary_ = new QLabel; right->addWidget(summary_);
    connect(name_, &QLineEdit::textChanged, this, [this] { if (!updating_) { pending_ = true; updateCodeStatus(); } });
    connect(pronto_, &QPlainTextEdit::textChanged, this, [this] { if (!updating_) { pending_ = true; updateCodeStatus(); } });
    connect(apply_, &QPushButton::clicked, this, [this] { applyEditor(); });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *now) {
        if (updating_) return;
        int index = now ? now->data(0, Qt::UserRole).toInt() : -1;
        if (!applyEditor()) {
            QSignalBlocker blocker(tree_);
            for (QTreeWidgetItemIterator it(tree_); *it; ++it) if ((*it)->data(0, Qt::UserRole).toInt() == selected_) { tree_->setCurrentItem(*it); break; }
            return;
        }
        // Selecting an entry does not rebuild the tree. A pending edit may
        // have refreshed its rows, so resolve the target by its model index.
        {
            QSignalBlocker blocker(tree_);
            for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
                if ((*it)->data(0, Qt::UserRole).toInt() != index) continue;
                tree_->setCurrentItem(*it);
                tree_->scrollToItem(*it);
                break;
            }
        }
        select(index);
    });
    connect(slotBox_, &QComboBox::currentIndexChanged, this, [this](int slot) {
        if (updating_) return;
        if (!applyEditor()) { QSignalBlocker block(slotBox_); slotBox_->setCurrentIndex(slot_); return; }
        slot_ = slot; select(selected_);
    });
    connect(import_, &QPushButton::clicked, this, [this] {
        auto f = QFileDialog::getOpenFileName(this, text("Import Pronto file"), {}, text("Pronto codes (*.pronto *.txt);;All files (*)")); if (f.isEmpty()) return;
        try { QFile input(f); if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error(input.errorString().toStdString()); if (input.size() > 1024*1024) throw std::runtime_error(text("Pronto file is too large.").toStdString()); auto p = Pronto::parse(QString::fromUtf8(input.readAll())); pronto_->setPlainText(p.text()); } catch (const std::exception &e) { error(e); }
    });
    connect(copy_, &QPushButton::clicked, this, [this] { try { QApplication::clipboard()->setText(Pronto::parse(pronto_->toPlainText()).text()); } catch (const std::exception &e) { error(e); } });
    connect(clear_, &QPushButton::clicked, this, [this] {
        if (!applyEditor() || selected_ < 0) return;
        auto after = document_; after.setCode(selected_, slot_, {}); change(after, selected_, text("Clear IR signal"));
    });
    connect(undo_, &QUndoStack::cleanChanged, this, [this] { retranslate(); });
    retranslate(); refresh(-1);
}
void EditorWindow::retranslate() {
    for (const auto &l : labels_) l.first->setProperty("text", text(l.second));
    tree_->setHeaderLabels({text("Device / command"), text("Signals")});
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        int index = (*it)->data(0, Qt::UserRole).toInt();
        if (index < 0 || index >= document_.items.size()) continue;
        if (document_.items[index].folder() || document_.items[index].macro()) (*it)->setText(1, document_.items[index].macro() ? text("Sequence") : text("Device / folder"));
    }
    slotBox_->setItemText(0, text("A · primary")); slotBox_->setItemText(1, text("B · toggle / alternate"));
    pronto_->setPlaceholderText(text("Paste Pronto Hex here. Use Clear signal to remove stored data."));
    setWindowTitle(QString("qt_iredit — %1%2").arg(file_.isEmpty() ? text("New IR menu") : QFileInfo(file_).fileName(), undo_->isClean() && !pending_ ? "" : " *"));
    updateCodeStatus();
}
void EditorWindow::refresh(int selected) {
    const int scroll = tree_->verticalScrollBar()->value();
    QSet<int> expanded;
    int oldCount = 0;
    bool sameStructure = true;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        auto *n = *it; ++oldCount;
        const int index = n->data(0, Qt::UserRole).toInt();
        const int parent = n->parent() ? n->parent()->data(0, Qt::UserRole).toInt()+1 : 0;
        if (index < 0 || index >= document_.items.size() || parent != document_.items[index].parent || n->data(0, Qt::UserRole+1).toInt() != (document_.items[index].flags & 0x50)) sameStructure = false;
        if (n->isExpanded()) expanded.insert(index);
    }
    sameStructure = sameStructure && oldCount == document_.items.size();
    updating_ = true; QSignalBlocker block(tree_); tree_->clear(); QVector<QTreeWidgetItem *> nodes;
    for (int i = 0; i < document_.items.size(); ++i) {
        const auto &entry = document_.items[i]; auto *n = new QTreeWidgetItem; n->setText(0, entry.name); n->setData(0, Qt::UserRole, i); n->setData(0, Qt::UserRole+1, entry.flags & 0x50);
        if (entry.folder() || entry.macro()) n->setText(1, entry.macro() ? text("Sequence") : text("Device / folder"));
        else n->setText(1, entry.codes[0].isEmpty() && entry.codes[1].isEmpty() ? "—" : (entry.codes[0].isEmpty() ? "B" : (entry.codes[1].isEmpty() ? "A" : "A + B")));
        if (entry.parent) nodes[entry.parent-1]->addChild(n); else tree_->addTopLevelItem(n); nodes.append(n);
    }
    if (sameStructure) {
        for (int i=0; i<nodes.size(); ++i) nodes[i]->setExpanded(expanded.contains(i));
    } else if (expandAll_) tree_->expandAll(); else tree_->collapseAll();
    tree_->verticalScrollBar()->setValue(scroll);
    if (selected >= 0 && selected < nodes.size()) { tree_->setCurrentItem(nodes[selected]); tree_->scrollToItem(nodes[selected]); }
    updating_ = false; select(selected); retranslate();
    statusBar()->showMessage(file_.isEmpty() ? text("Local files only · no controller connection") : file_);
}
void EditorWindow::select(int index) {
    updating_ = true; selected_ = index >= 0 && index < document_.items.size() ? index : -1; pending_ = false;
    bool exists = selected_ >= 0, command = exists && !document_.items[selected_].folder() && !document_.items[selected_].macro();
    name_->setEnabled(exists); name_->setText(exists ? document_.items[selected_].name : QString());
    slotBox_->setEnabled(command); pronto_->setEnabled(command); raw_->setEnabled(command); import_->setEnabled(command); copy_->setEnabled(command); clear_->setEnabled(command);
    auto code = command ? document_.items[selected_].codes[slot_] : QByteArray();
    pronto_->setPlainText(Pronto::fromRnet(code)); raw_->setPlainText(QString::fromLatin1(code.toHex(' ').toUpper()));
    updating_ = false; updateCodeStatus();
}
void EditorWindow::updateCodeStatus() {
    if (!codeStatus_) return;
    bool command = selected_ >= 0 && !document_.items[selected_].folder() && !document_.items[selected_].macro();
    bool valid = true; QString detail;
    if (!command) detail = text("Select a command to edit its signal.");
    else if (pronto_->toPlainText().trimmed().isEmpty()) detail = document_.items[selected_].codes[slot_].isEmpty() ? text("No IR code stored for this slot.") : text("Stored signal retained. Paste Pronto Hex to replace it.");
    else try {
        auto p = Pronto::parse(pronto_->toPlainText()); auto bytes = p.toRnet();
        detail = text("%1 kHz · %2 pulse pairs · %3 R-net bytes").arg(p.frequency()/1000.0, 0, 'f', 2).arg(p.pairs()).arg(bytes.size());
    } catch (const std::exception &e) { valid = false; detail = QString::fromUtf8(e.what()); }
    if (selected_ >= 0 && name_->text().trimmed().isEmpty()) { valid = false; detail = text("Enter an entry name."); }
    codeStatus_->setText(detail); codeStatus_->setStyleSheet(valid ? "" : "color: #bd3b32;");
    apply_->setEnabled(selected_ >= 0 && pending_ && valid);
    int count = 0; for (const auto &i : document_.items) if (!i.codes[0].isEmpty() || !i.codes[1].isEmpty()) ++count;
    summary_->setText(text("%1 menu entries · %2 commands with stored signals").arg(document_.items.size()).arg(count));
    setWindowTitle(QString("qt_iredit — %1%2").arg(file_.isEmpty() ? text("New IR menu") : QFileInfo(file_).fileName(), undo_->isClean() && !pending_ ? "" : " *"));
}
bool EditorWindow::applyEditor() {
    if (!pending_ || selected_ < 0) return true;
    try {
        auto after = document_; after.items[selected_].name = name_->text().trimmed();
        if (!after.items[selected_].folder() && !after.items[selected_].macro() && !pronto_->toPlainText().trimmed().isEmpty()) {
            auto code = Pronto::parse(pronto_->toPlainText()).toRnet();
            if (code != after.items[selected_].codes[slot_]) after.setCode(selected_, slot_, code);
        }
        after.validate(); change(after, selected_, text("Edit IR entry")); return true;
    } catch (const std::exception &e) { error(e); return false; }
}
void EditorWindow::change(const Document &after, int selected, const QString &label) {
    if (document_.json() == after.json()) { pending_ = false; refresh(selected); return; }
    undo_->push(new Edit([this](const Document &d, int index) { document_ = d; pending_ = false; refresh(index); }, document_, after, selected_, selected, label));
}
void EditorWindow::openFile(const QString &file) {
    auto document = Document::load(file); document_ = std::move(document); file_ = QFileInfo(file).absoluteFilePath(); undo_->clear(); pending_ = false; slot_ = 0; { QSignalBlocker block(slotBox_); slotBox_->setCurrentIndex(0); }
    int selected = document_.items.isEmpty() ? -1 : 0; for (int i = 0; i < document_.items.size(); ++i) if (!document_.items[i].codes[0].isEmpty()) { selected = i; break; }
    refresh(selected);
}
bool EditorWindow::save(bool chooseFile) {
    if (!applyEditor()) return false;
    QString file = file_;
    if (chooseFile || file.isEmpty()) {
        file = QFileDialog::getSaveFileName(this, text("Save IR menu"), file.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/IR-menu.RnIR3" : file, text("R-net IR menu (*.RnIR3);;IR editor project (*.json)"));
        if (file.isEmpty()) return false;
        if (QFileInfo(file).suffix().isEmpty()) file += ".RnIR3";
    }
    try { document_.save(file); file_ = file; undo_->setClean(); retranslate(); statusBar()->showMessage(text("Saved locally: %1").arg(file_)); return true; } catch (const std::exception &e) { error(e); return false; }
}
bool EditorWindow::maybeSave() {
    if (undo_->isClean() && !pending_) return true;
    auto answer = QMessageBox::question(this, text("Unsaved IR menu"), text("Save changes to this IR menu?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return answer == QMessageBox::Discard || (answer == QMessageBox::Save && save(false));
}
void EditorWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
void EditorWindow::addDevice() {
    if (!applyEditor()) return;
    bool ok = false; auto name = QInputDialog::getText(this, text("Add device"), text("Device name"), QLineEdit::Normal, {}, &ok); if (!ok || name.trimmed().isEmpty()) return;
    try { auto after = document_; int i = after.addFolder(name.trimmed()); change(after, i, text("Add IR device")); } catch (const std::exception &e) { error(e); }
}
void EditorWindow::addCommand() {
    if (!applyEditor()) return;
    int parent = selected_ < 0 ? -1 : (document_.items[selected_].folder() ? selected_ : int(document_.items[selected_].parent)-1);
    if (parent < 0) { QMessageBox::information(this, text("Add command"), text("Select a device or folder first.")); return; }
    bool ok = false; auto name = QInputDialog::getText(this, text("Add command"), text("Command name"), QLineEdit::Normal, {}, &ok); if (!ok || name.trimmed().isEmpty()) return;
    try { auto after = document_; int i = after.addCommand(name.trimmed(), parent); change(after, i, text("Add IR command")); } catch (const std::exception &e) { error(e); }
}
void EditorWindow::removeEntry() {
    if (!applyEditor() || selected_ < 0) return;
    auto after = document_; after.remove(selected_); change(after, after.items.isEmpty() ? -1 : qMin(selected_, int(after.items.size())-1), text("Remove IR entry"));
}
void EditorWindow::error(const std::exception &e) { QMessageBox::warning(this, text("IR editor"), QString::fromUtf8(e.what())); }
}
