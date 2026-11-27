#pragma once
#include "ir_document.hpp"
#include <QMainWindow>
#include <functional>
#include <stdexcept>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QComboBox;
class QPushButton;
class QUndoStack;
namespace iredit {
class EditorWindow : public QMainWindow {
public:
    explicit EditorWindow(QWidget *parent = nullptr);
    void openFile(const QString &file);
    void retranslate();
    std::function<void(const QString &)> changeLanguage;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Document document_;
    QString file_;
    int selected_ = -1, slot_ = 0;
    bool pending_ = false, updating_ = false, expandAll_ = true;
    QTreeWidget *tree_;
    QLineEdit *name_;
    QPlainTextEdit *pronto_, *raw_;
    QLabel *summary_, *codeStatus_;
    QComboBox *slotBox_;
    QPushButton *apply_, *clear_, *import_, *copy_;
    QUndoStack *undo_;
    QList<QPair<QObject *, const char *>> labels_;
    void bind(QObject *object, const char *text);
    void refresh(int selected);
    void select(int selected);
    void updateCodeStatus();
    bool applyEditor();
    void change(const Document &after, int selected, const QString &label);
    bool maybeSave();
    bool save(bool chooseFile);
    void addDevice();
    void addCommand();
    void removeEntry();
    void error(const std::exception &e);
};
}
