#pragma once
#include "attendant_state.hpp"

#include "can_observer.hpp"
#include "can_transmitter.hpp"
#include "can_control.hpp"
#include "startup_config.hpp"
#include "startup_ready.hpp"
#include "module_enrollment.hpp"
#include "registered_resume.hpp"
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QString>
namespace attendant {
class AttendantController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString benchSerial READ benchSerial NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(bool attendantControl READ attendantControl NOTIFY changed)
    Q_PROPERTY(bool held READ held NOTIFY changed)
    Q_PROPERTY(bool controlsEnabled READ controlsEnabled NOTIFY changed)
    Q_PROPERTY(bool controlSuspended READ controlSuspended NOTIFY changed)
    Q_PROPERTY(int profile READ profile CONSTANT)
    Q_PROPERTY(int modeNumber READ modeNumber NOTIFY changed)
    Q_PROPERTY(int inputX READ inputX NOTIFY changed)
    Q_PROPERTY(int inputY READ inputY NOTIFY changed)
    Q_PROPERTY(int outputX READ outputX NOTIFY changed)
    Q_PROPERTY(int outputY READ outputY NOTIFY changed)
    Q_PROPERTY(int inputLimit READ inputLimit NOTIFY changed)
    Q_PROPERTY(QString canInterface READ canInterface WRITE setCanInterface NOTIFY changed)
    Q_PROPERTY(bool canConnected READ canConnected NOTIFY changed)
    Q_PROPERTY(bool canTxAllowed READ canTxAllowed CONSTANT)
    Q_PROPERTY(bool canTxConnected READ canTxConnected NOTIFY changed)
    Q_PROPERTY(QString canTxCount READ canTxCount NOTIFY changed)
    Q_PROPERTY(QString canCount READ canCount NOTIFY changed)
public:
    explicit AttendantController(QObject *parent=nullptr);
    ~AttendantController() override;
    QString benchSerial() const { return txAllowed_ ? (moduleReady() ? QString::fromLatin1(moduleIdentity_.toHex()) : QString()) : (engine_.state().linked ? simulatedSerial_ : QString()); }
    QString statusText() const;
    QString detail() const{return detail_;}
    bool powered() const{return effective().power;}
    bool attendantControl() const{return effective().attendant;}
    bool held() const{return effective().held;}
    bool controlsEnabled() const{return !controlSuspended_&&!closing_&&!powerDown_&&!registrationOnly_&&!liveEnrollmentBlocked_&&effective().linked&&active_&&(!txAllowed_||(moduleReady()&&!canControl_.pending()));}
    bool controlSuspended() const{return controlSuspended_;}
    bool canResumeControl() const{return !closing_&&(!controlSuspended_||suspendReturnConfirmed_)&&(!txAllowed_||(!canControl_.pending()&&!canControl_.owned()));}
    bool suspensionComplete() const{return controlSuspended_&&suspendReturnConfirmed_;}
    Q_INVOKABLE void setControlSuspended(bool suspended);
    int profile() const { return 8; }
    int modeNumber() const { return txAllowed_ ? canControl_.mode() + 1 : simulatedMode_ + 1; }
    int inputX() const{return desired_.x;}
    int inputY() const{return desired_.y;}
    int outputX() const{return effective().x;}
    int outputY() const{return effective().y;}
    int inputLimit() const{return desired_.limit;}
    QString canInterface() const{return interface_;}
    void setCanInterface(const QString &name);
    bool canConnected() const{return observer_.connected();}
    QString canCount() const{return QString::number(observer_.count());}
    bool canTxAllowed() const {return txAllowed_;}
    bool canTxConnected() const {return transmitter_.connected();}
    QString canTxCount() const {return QString::number(transmitter_.count());}
    void blockLiveEnrollment();
    bool setEnrollmentProfile(const QJsonObject &profile);
    void requestInitialEnrollment() { enrollment_.requestInitialEnrollment(); }
    void setModuleIdentity(const QByteArray &identity) { if (identity.size() == 8) moduleIdentity_ = identity; }
    void setRegistrationOnly(bool enabled) { registrationOnly_ = enabled; }
    void allowCanTx(bool allowed) {txAllowed_=allowed;}
    Q_INVOKABLE void startCanTx();
    Q_INVOKABLE void stopCanTx();
    Q_INVOKABLE void setAxes(double x,double y);
    Q_INVOKABLE void toggleBenchEnable();
    Q_INVOKABLE void toggleDrivingControl();
    Q_INVOKABLE void nextProfile();
    Q_INVOKABLE void nextSpeed();
    Q_INVOKABLE void setHold(bool held);
    Q_INVOKABLE void requestControl();
    Q_INVOKABLE void setPower(bool on);
    Q_INVOKABLE void setInputLimit(int percent);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void observeCan();
    Q_INVOKABLE void stopCanObservation();
    Q_INVOKABLE void chooseLanguage(const QString &language){emit languageRequested(language);}
    void registerOnDataFrame();
    void setApplicationActive(bool active);
    Q_INVOKABLE bool requestClose();
    void systemPowerOffRequested();
    Q_INVOKABLE void shutdown();
signals:
    void changed();
    void languageRequested(QString language);
    void closeReady();
private:
    QString interface_="can0",detail_;
    bool active_=true;
    bool controlSuspended_=false;
    bool suspendReturnConfirmed_=true;
    const QString simulatedSerial_;
    Input desired_;

    AttendantState engine_;

    CanObserver observer_;
    CanTransmitter transmitter_;
    bool txAllowed_=false;
    bool liveEnrollmentBlocked_=false;
    bool registrationOnly_=false;
    bool returnFailed_=false, controlFailed_=false;
    bool inputDirty_=false, powerDown_=false, closing_=false, observing_=false;
    bool observerWasConnected_=false;
    bool closeReturnConfirmed_=false;
    qint64 lastObserverCheck_=0, lastObserverRetry_=0;
    qint64 closeDeadline_=0;
    bool automaticEnable_=false;
    int simulatedMode_=0;
    void enableWhenCentered();
    CanControl canControl_;
    StartupConfig startup_;
    StartupReady readiness_;
    ModuleEnrollment enrollment_;
    RegisteredResume resume_;
    bool moduleReady() const {return enrollment_.error().isEmpty() && (resume_.ready() || (readiness_.ready()&&enrollment_.complete()&&startup_.state()==StartupConfig::State::Ready));}
    QByteArray moduleIdentity_;
    qint64 lastControllerFrame_ = -1;
    void sendFrames(const QStringList &frames);
    void receiveFrame(quint32 id, const QByteArray &data);
    QElapsedTimer clock_;
    QTimer timer_;
    const State &effective() const{return engine_.state();}
    bool advanceRequest();
    void pulse();

};
}
