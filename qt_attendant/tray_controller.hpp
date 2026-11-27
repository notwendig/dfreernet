#pragma once
#include <QObject>
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <functional>

class QQuickWindow;
class QSettings;
namespace attendant {
class AttendantController;
class TrayController final : public QObject {
    Q_OBJECT
public:
    struct Options { bool startHidden=false, minimizeToTray=false, disabled=false; };
    TrayController(QQuickWindow *window, AttendantController *controller,
                   QSettings &settings, Options options,
                   std::function<bool()> available=QSystemTrayIcon::isSystemTrayAvailable);
    bool available() const { return available_; }
    bool startHidden() const { return startHidden_; }
    bool minimizeToTray() const { return minimizeToTray_; }
    QMenu *menu() { return &menu_; }
    void showWindow();
    void hideWindow();
    void requestQuit();
    void retranslate();
    void checkAvailability();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
signals:
    void quitReady();
private:
    QPointer<QQuickWindow> window_;
    AttendantController *controller_;
    QSettings &settings_;
    std::function<bool()> probe_;
    bool available_=false, startHidden_=false, minimizeToTray_=false;
    bool quitting_=false, hideAfterSuspend_=false, startHiddenPending_=false;
    QMenu menu_;
    QSystemTrayIcon icon_;
    QAction *show_, *hide_, *suspend_, *start_, *minimize_, *quit_;
    QTimer availabilityTimer_;
    QTimer suspendHideTimer_;
    void refresh();
};
}
