#include "tray_controller.hpp"
#include "attendant_controller.hpp"
#include "app_icon.hpp"
#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QQuickWindow>
#include <QSettings>

namespace attendant {
namespace {
QString text(const char *s) { return QCoreApplication::translate("RNet",s); }
}
TrayController::TrayController(QQuickWindow *window, AttendantController *controller,
                               QSettings &settings, Options options,
                               std::function<bool()> available)
    :window_(window),controller_(controller),settings_(settings),probe_(std::move(available)) {
    startHidden_=!options.disabled && (options.startHidden || settings_.value("tray/startHidden",false).toBool());
    minimizeToTray_=!options.disabled && (options.minimizeToTray || settings_.value("tray/minimize",false).toBool());
    show_=menu_.addAction(QString());show_->setObjectName("trayShow");
    hide_=menu_.addAction(QString());hide_->setObjectName("trayHide");
    suspend_=menu_.addAction(QString());suspend_->setObjectName("traySuspend");
    menu_.addSeparator();
    start_=menu_.addAction(QString());start_->setObjectName("trayStartHidden");start_->setCheckable(true);start_->setChecked(startHidden_);
    minimize_=menu_.addAction(QString());minimize_->setObjectName("trayMinimize");minimize_->setCheckable(true);minimize_->setChecked(minimizeToTray_);
    menu_.addSeparator();
    quit_=menu_.addAction(QString());quit_->setObjectName("trayQuit");
    connect(show_,&QAction::triggered,this,&TrayController::showWindow);
    connect(hide_,&QAction::triggered,this,&TrayController::hideWindow);
    suspendHideTimer_.setSingleShot(true);
    connect(&suspendHideTimer_,&QTimer::timeout,this,[this]{
        if(hideAfterSuspend_ && controller_->suspensionComplete()) {
            hideAfterSuspend_=false;
            hideWindow();
        }
    });
    connect(suspend_,&QAction::triggered,this,[this]{
        if(controller_->controlSuspended()) {
            controller_->setControlSuspended(false);
            if(!controller_->controlSuspended())showWindow();
        } else {
            hideAfterSuspend_=true;
            controller_->setControlSuspended(true);
        }
        refresh();
    });
    connect(start_,&QAction::toggled,this,[this](bool checked){startHidden_=checked;settings_.setValue("tray/startHidden",checked);});
    connect(minimize_,&QAction::toggled,this,[this](bool checked){minimizeToTray_=checked;settings_.setValue("tray/minimize",checked);});
    connect(quit_,&QAction::triggered,this,&TrayController::requestQuit);
    connect(controller_,&AttendantController::changed,this,&TrayController::refresh);
    icon_.setIcon(applicationIcon());icon_.setContextMenu(&menu_);
    connect(&icon_,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){
        if(reason==QSystemTrayIcon::Trigger || reason==QSystemTrayIcon::DoubleClick)showWindow();
    });
    connect(window,&QWindow::windowStateChanged,this,[this](Qt::WindowState state){
        if(state==Qt::WindowMinimized && minimizeToTray_)hideWindow();
    });
    if(options.disabled)probe_=[] {return false;};
    else window->installEventFilter(this);
    available_=probe_();
    startHiddenPending_=startHidden_&&!available_;
    if(available_)icon_.show();
    else if(startHidden_)qWarning("System tray unavailable; keeping the window visible.");
    connect(&availabilityTimer_,&QTimer::timeout,this,&TrayController::checkAvailability);
    availabilityTimer_.start(2000);
    retranslate();
    if(startHidden_ && available_)hideWindow();
}
void TrayController::retranslate() {
    show_->setText(text("Show window"));hide_->setText(text("Hide to tray"));
    start_->setText(text("Start in system tray"));
    minimize_->setText(text("Minimize to system tray"));quit_->setText(text("Quit"));
    refresh();
}
void TrayController::refresh() {
    if(!controller_->controlSuspended()) {
        hideAfterSuspend_=false;
        suspendHideTimer_.stop();
    }
    suspend_->setText(controller_->controlSuspended()?text("Resume control"):text("Suspend control"));
    suspend_->setEnabled(!controller_->controlSuspended() || controller_->canResumeControl());
    const auto tip=text("R-Net Control")+"\n"+controller_->statusText();
    if(icon_.toolTip()!=tip)icon_.setToolTip(tip);
    if(hideAfterSuspend_ && controller_->suspensionComplete() && !suspendHideTimer_.isActive())
        suspendHideTimer_.start(0);
}
void TrayController::showWindow() {
    startHiddenPending_=hideAfterSuspend_=false;
    suspendHideTimer_.stop();
    if(!window_)return;
    window_->showNormal();window_->raise();window_->requestActivate();
}
void TrayController::hideWindow() {
    checkAvailability();
    if(!window_ || !available_)return;
    controller_->setApplicationActive(false);
    window_->hide();
}
void TrayController::requestQuit() {
    // Restore progress feedback and explicitly quit only after QML accepts
    // close. A hidden-window session cannot rely on lastWindowClosed alone.
    quitting_=true;
    showWindow();
    if(window_ && window_->close())emit quitReady();
}
void TrayController::checkAvailability() {
    const bool now=probe_();
    if(now==available_)return;
    available_=now;icon_.setVisible(now);
    if(!now && window_ && !window_->isVisible()) {
        qWarning("System tray disappeared; restoring the window.");
        showWindow();
    }
    if(now && startHiddenPending_) {
        startHiddenPending_=false;
        hideWindow();
    }
}
bool TrayController::eventFilter(QObject *object,QEvent *event) {
    if(object==window_ && event->type()==QEvent::Close && !quitting_) {
        event->ignore();
        hideWindow();
        if(!available_)qWarning("System tray unavailable; keeping the window visible.");
        return true;
    }
    return QObject::eventFilter(object,event);
}
}
