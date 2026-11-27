#include "attendant_controller.hpp"
#include <QCoreApplication>
#include <QUuid>

#include <QGuiApplication>
#include <algorithm>
#include <cmath>
namespace attendant {
namespace {QString text(const char *s){return QCoreApplication::translate("RNet",s);}}
AttendantController::AttendantController(QObject *parent):QObject(parent),simulatedSerial_("SIM-" + QUuid::createUuid().toString(QUuid::Id128).left(12)),observer_(this),transmitter_(this){
    connect(&transmitter_,&CanTransmitter::changed,this,[this]{if(!transmitter_.error().isEmpty())detail_=transmitter_.error();emit changed();});
    clock_.start();
    connect(&observer_, &CanObserver::protocolFrame, this, [this](quint32 id, const QByteArray &data) {
        if (!txAllowed_ || powerDown_) return;
        sendFrames(enrollment_.receive(id, data, clock_.elapsed()));
        if (enrollment_.configurationRequested() && startup_.state() == StartupConfig::State::Waiting)
            sendFrames({startup_.begin(clock_.elapsed())});
        emit changed();
    });
    connect(&observer_, &CanObserver::powerOffRequested, this, &AttendantController::systemPowerOffRequested);
    connect(&observer_, &CanObserver::dataFrame, this, &AttendantController::receiveFrame);
    connect(&observer_, &CanObserver::firstDataFrame, this, &AttendantController::registerOnDataFrame);
    connect(&observer_, &CanObserver::changed, this, [this] {
        if (!observer_.connected()) {
            if (observing_) powerDown_ = true;
            automaticEnable_ = false;
            enrollment_.reset();
            startup_.reset();
            readiness_.reset();
            canControl_.reset();
            engine_.endSession();
            desired_ = {};
            transmitter_.close();
        }
        emit changed();
    });
    timer_.setInterval(10);timer_.setTimerType(Qt::PreciseTimer);connect(&timer_,&QTimer::timeout,this,&AttendantController::pulse);timer_.start();
}
AttendantController::~AttendantController(){shutdown();observer_.disconnect(this);transmitter_.disconnect(this);}
void AttendantController::blockLiveEnrollment(){
    stop();
    enrollment_.reset();
    startup_.reset();
    readiness_.reset();
    txAllowed_ = false;
    liveEnrollmentBlocked_ = true;
    detail_ = text("Live enrollment disabled after DIME error 8105");
    emit changed();
}
bool AttendantController::setEnrollmentProfile(const QJsonObject &profile){
    if (enrollment_.configure(profile)) {
        setModuleIdentity(QByteArray::fromHex(profile.value("identity").toString().toLatin1()));
        return enrollment_.liveProfileReady();
    } else {
        detail_ = text("Invalid module enrollment profile");
    }
    return false;
}
void AttendantController::registerOnDataFrame(){
    if (powerDown_ || closing_ || effective().linked) return;
    engine_.beginSession(clock_.elapsed());
    if (txAllowed_) startCanTx();
    emit changed();
}
void AttendantController::setCanInterface(const QString &name){if(observer_.connected()||transmitter_.connected())return;interface_=name.trimmed();emit changed();}
bool AttendantController::advanceRequest(){
    if(desired_.request>=2147483647u){shutdown();detail_=text("Control counter exhausted. Start a new session.");emit changed();return false;}
    ++desired_.request;inputDirty_=true;return true;
}
void AttendantController::setAxes(double x,double y){
    if(!controlsEnabled()){
        if (x == 0 && y == 0) {
            desired_.x = desired_.y = 0;
            inputDirty_ = true;
            emit changed();
        }
        return;
    }
    if(!std::isfinite(x)||!std::isfinite(y)){stop();return;}
    desired_.x=std::lround(std::clamp(x,-100.0,100.0));desired_.y=std::lround(std::clamp(y,-100.0,100.0));inputDirty_=true;pulse();
    enableWhenCentered();
}
void AttendantController::enableWhenCentered(){
    if (!automaticEnable_ || !attendantControl() || held() ||
        std::abs(desired_.x) > 5 || std::abs(desired_.y) > 5) return;
    setHold(false);
    setHold(true);
}
void AttendantController::toggleDrivingControl(){
    if (!controlsEnabled()) return;
    if (txAllowed_) {
        if (canControl_.pending()) return;
        if (!transmitter_.connected()) startCanTx();
        if (!transmitter_.connected()) return;
        if (!canControl_.owned() && (std::abs(desired_.x) > 5 || std::abs(desired_.y) > 5)) {
            detail_ = text("Center the joystick before taking control.");
            emit changed();
            return;
        }
        engine_.stop();
        desired_.attendant = desired_.hold = automaticEnable_ = false;
        returnFailed_ = controlFailed_ = false;
        sendFrames(canControl_.request(!canControl_.owned(), clock_.elapsed()));
        emit changed();
        return;
    }
    setHold(false);
    requestControl();
    automaticEnable_ = attendantControl();
    enableWhenCentered();
}
void AttendantController::nextProfile(){
    if (!controlsEnabled()) return;
    if (txAllowed_) {
        engine_.stop();
        desired_.attendant = desired_.hold = automaticEnable_ = false;
        controlFailed_ = false;
        sendFrames(canControl_.changeMode(clock_.elapsed()));
        emit changed();
        return;
    }
    setHold(false);
    simulatedMode_ = (simulatedMode_ + 1) % 2;
    enableWhenCentered();
    emit changed();
}
void AttendantController::nextSpeed(){
    if (!controlsEnabled()) return;
    setHold(false);
    setInputLimit(inputLimit() >= 100 ? 0 : (inputLimit() / 20 + 1) * 20);
    enableWhenCentered();
    if (txAllowed_) sendFrames(canControl_.speed(inputLimit()));
}
void AttendantController::toggleBenchEnable(){
    if (!controlsEnabled() || !attendantControl()) return;
    setHold(!held());
    if (!held() && desired_.hold) setHold(false);
}

void AttendantController::setHold(bool hold){if(hold&&!controlsEnabled())return;desired_.hold=hold;inputDirty_=true;pulse();}
void AttendantController::requestControl(){if(!controlsEnabled())return;desired_.attendant=!attendantControl();if(advanceRequest())pulse();}
void AttendantController::setPower(bool on){if(!controlsEnabled())return;desired_.power=on;desired_.attendant=false;desired_.hold=false;if(advanceRequest())pulse();}
void AttendantController::setInputLimit(int percent){if(!controlsEnabled())return;desired_.limit=std::clamp(percent,0,100);inputDirty_=true;pulse();}
void AttendantController::startCanTx(){
    if(!txAllowed_||powerDown_||!effective().linked)return;
    if(transmitter_.open(interface_))detail_=text("CAN transmitter ready");
    else {
        detail_=transmitter_.error();
        enrollment_.fail(detail_);
        canControl_.reset();
        engine_.stop(Reason::LinkLost);
    }
    emit changed();
}
void AttendantController::stopCanTx(){transmitter_.close();emit changed();}
void AttendantController::sendFrames(const QStringList &frames){
    if (!txAllowed_ || powerDown_) return;
    for (const auto &frame : frames) {
        if (!transmitter_.send(frame)) {
            enrollment_.fail(transmitter_.error());
            canControl_.reset();
            engine_.stop();
            automaticEnable_ = false;
            desired_.attendant = desired_.hold = false;
            break;
        }
    }
}
void AttendantController::receiveFrame(quint32 id, const QByteArray &data){
    if (!txAllowed_) return;
    if (powerDown_ && id != 0x00c) return;
    if (id == 0x00c && data.isEmpty()) powerDown_ = false;
    if (!effective().linked && id != 0) registerOnDataFrame();
    if (id == 0x00c && data.isEmpty()) {
        returnFailed_ = controlFailed_ = false;
        canControl_.reset();
        stop();
        startup_.reset();
        readiness_.reset();
        enrollment_.boot(clock_.elapsed());
        if (enrollment_.configured()) startCanTx();
        return;
    }
    if (id == 0 && data.isEmpty()) {
        systemPowerOffRequested();
        return;
    }
    if (id == 0x1fb00000 && data.size() == 8 && !moduleIdentity_.isEmpty() && data != moduleIdentity_) {
        stop();
        startup_.reset();
        readiness_.reset();
        enrollment_.fail(text("Slot 0 contains another device"));
        detail_ = text("Slot 0 contains another device");
        emit changed();
        return;
    }
    const auto nextRequest = startup_.receive(id, data, clock_.elapsed());
    if (!nextRequest.isEmpty()) sendFrames({nextRequest});
    if (enrollment_.registeredRestart() && startup_.state() == StartupConfig::State::Ready) {
        if (readiness_.state() == StartupReady::State::Waiting)
            sendFrames(readiness_.begin(clock_.elapsed()));
        sendFrames(readiness_.receive(id,data,clock_.elapsed()));
    }
    if (id == 0x02000000 && readiness_.ready()) {
        stop();
        enrollment_.fail(text("Another device is sending joystick slot 0"));
        detail_ = text("Another device is sending joystick slot 0");
        return;
    }
    if (id == 0x063 || id == 0x00e || id == 0x03c30f0f)
        lastControllerFrame_ = clock_.elapsed();
    bool controlChanged = false;
    if (readiness_.ready() && startup_.state() == StartupConfig::State::Ready) {
        const bool returning=canControl_.returning();
        controlChanged = canControl_.receive(id, data, clock_.elapsed());
        sendFrames(canControl_.takeOutgoing());
        if (controlChanged && returning && !canControl_.owned() && !canControl_.pending())
            closeReturnConfirmed_=true;
    }
    if (controlChanged) {
        const bool owns = canControl_.owned();
        engine_.stop();
        if (closing_ && owns) {
            sendFrames(canControl_.request(false,clock_.elapsed()));
            emit changed();
            return;
        }
        desired_.hold = false;
        desired_.attendant = owns;
        automaticEnable_ = owns;
        if (advanceRequest()) pulse();
        enableWhenCentered();
        if (owns) sendFrames(canControl_.speed(inputLimit()));
        emit changed();
    }
}
void AttendantController::stop(){
    if (txAllowed_ && canControl_.owned())
        sendFrames(canControl_.request(false, clock_.elapsed()));
    canControl_.reset();
    automaticEnable_ = false;
    transmitter_.close();
    desired_.x=desired_.y=0;desired_.hold=false;desired_.attendant=false;advanceRequest();engine_.stop();
    emit changed();
}
void AttendantController::pulse(){
    const auto time=clock_.elapsed();
    if (observing_ && observer_.connected() && time-lastObserverCheck_>=250) {
        lastObserverCheck_=time;
        observer_.checkInterface();
    }
    if (observing_ && !observer_.connected() && time-lastObserverRetry_>=1000) {
        lastObserverRetry_=time;
        observer_.open(interface_);
    }
    if (effective().linked) {
        if (inputDirty_) {
            inputDirty_ = false;
            engine_.apply(desired_,clock_.elapsed());
        } else engine_.tick(clock_.elapsed());
    }
    if (txAllowed_) {
        const auto now = clock_.elapsed();
        sendFrames(enrollment_.tick(now));
        if (enrollment_.configurationRequested() && startup_.state() == StartupConfig::State::Waiting)
            sendFrames({startup_.begin(now)});
        if (!enrollment_.error().isEmpty() && transmitter_.connected()) {
            stop();
            detail_ = QCoreApplication::translate("RNet", enrollment_.error().toUtf8().constData());
        }
        startup_.tick(now);
        readiness_.tick(now);
        if (readiness_.state() == StartupReady::State::Failed && transmitter_.connected()) {
            stop();
            detail_ = QCoreApplication::translate("RNet", readiness_.error().toUtf8().constData());
        }
        if (startup_.state() == StartupConfig::State::Failed && transmitter_.connected()) {
            stop();
            detail_ = QCoreApplication::translate("RNet", startup_.error().toUtf8().constData());
        }
        if (canControl_.owned() && effective().reason == Reason::Timeout) {
            sendFrames(canControl_.request(false,now));
            desired_.attendant = desired_.hold = automaticEnable_ = false;
        }
        if (canControl_.expired(now) && canControl_.returning()) returnFailed_ = true;
        if (canControl_.expired(now) ||
            (canControl_.owned() && (lastControllerFrame_ < 0 || now - lastControllerFrame_ > 500))) {
            controlFailed_ = true;
            stop();
            detail_ = text("CAN control confirmation timed out");
        } else if (canControl_.owned()) {
            sendFrames({canControl_.joystick(outputX(), outputY())});
        }
    }
    if (closing_ && ((!canControl_.owned() && !canControl_.pending()) || clock_.elapsed() >= closeDeadline_)) {
        if (!closeReturnConfirmed_ || returnFailed_ || canControl_.owned() || canControl_.pending())
            qWarning("Controller did not confirm return before closing; output stopped.");
        shutdown();
        emit closeReady();
        return;
    }
    emit changed();
}
void AttendantController::observeCan(){
    observing_=true;
    lastObserverRetry_=clock_.elapsed();
    detail_.clear();
    if (!observer_.open(interface_)) detail_ = observer_.error();
    emit changed();
}
void AttendantController::stopCanObservation(){observing_=false;observer_.close();emit changed();}
void AttendantController::setApplicationActive(bool active){
    if (active_ == active) return;
    active_=active;
    // Focus is not a control-return request. Preserve the session and settings,
    // but never carry a displaced joystick into or out of the background.
    desired_.x = desired_.y = 0;
    inputDirty_ = true;
    pulse();
    emit changed();
}
bool AttendantController::requestClose(){
    if (!txAllowed_ || (!canControl_.owned() && !canControl_.pending())) {
        shutdown();
        return true;
    }
    if (!closing_) {
        closing_ = true;
        closeReturnConfirmed_ = false;
        closeDeadline_ = clock_.elapsed()+5000;
        desired_.x = desired_.y = 0;
        desired_.attendant = desired_.hold = automaticEnable_ = false;
        engine_.stop();
        if (canControl_.owned()) sendFrames(canControl_.request(false,clock_.elapsed()));
        emit changed();
    }
    return false;
}
void AttendantController::systemPowerOffRequested(){
    powerDown_ = true;
    enrollment_.reset();
    startup_.reset();
    readiness_.reset();
    canControl_.reset();
    transmitter_.close();
    desired_.x = desired_.y = 0;
    desired_.attendant = desired_.hold = automaticEnable_ = inputDirty_ = false;
    engine_.endSession();
    detail_ = text("CAN system stopped");
    emit changed();
}
void AttendantController::shutdown(){observing_=false;stop();transmitter_.close();timer_.stop();engine_.endSession();observer_.close();}
QString AttendantController::statusText() const{
    if (closing_) return text("Returning control before closing");
    if (!observer_.error().isEmpty()) return text("CAN interface is missing or down.");
    if (powerDown_) return text("Waiting for system power-on");
    if (observing_ && !txAllowed_) return text("CAN monitor; controls disabled");
    if (liveEnrollmentBlocked_) return text("Live enrollment disabled after DIME error 8105");
    const auto &s=effective();
    if (!s.linked) return text("Waiting for CAN data");
    if (txAllowed_ && !enrollment_.error().isEmpty()) return text("Module registration failed");
    if (txAllowed_ && startup_.state() == StartupConfig::State::Waiting) {
        if (enrollment_.running()) return text("Module registration in progress");
        return enrollment_.configured() ? text("Waiting for system power-on") : text("Module enrollment profile is missing");
    }
    if (txAllowed_ && startup_.state() == StartupConfig::State::Loading)
        return text("Loading module configuration");
    if (txAllowed_ && startup_.state() == StartupConfig::State::Failed)
        return text("Module configuration failed");
    if (txAllowed_ && enrollment_.running()) return text("Module registration in progress");
    if (txAllowed_ && readiness_.state() == StartupReady::State::Failed)
        return text("Module readiness failed");
    if (txAllowed_ && readiness_.running()) return text("Waiting for module readiness");
    if (txAllowed_ && !enrollment_.registeredRestart() && enrollment_.complete())
        return text("Registration transfer complete; restart the system");
    if (txAllowed_ && returnFailed_) return text("Return to passenger not confirmed");
    if (txAllowed_ && controlFailed_) return text("CAN control confirmation timed out");
    if (txAllowed_ && canControl_.returning()) return text("Waiting for passenger mode confirmation");
    if (txAllowed_ && canControl_.pending()) return text("Waiting for CAN control confirmation");
    if (txAllowed_ && canControl_.owned()) return text("CAN control confirmed");
    if (txAllowed_ && readiness_.ready()) return text("Module ready; passenger has control");
    if(!s.power)return text("Virtual module off");
    switch(s.reason){
    case Reason::CenterFirst:return text("Center the joystick before taking control.");
    case Reason::ReleaseFirst:return text("Release the enable button, then center the joystick.");
    case Reason::Timeout:return text("Input timed out · neutral · take control again");
    case Reason::LinkLost:return text("Connection lost · neutral");
    case Reason::FocusLost:return text("App inactive · neutral");
    case Reason::LimitChanged:return text("Input limit changed · release and re-enable");
    default:break;
    }
    if(s.attendant&&s.held)return text("Attendant input enabled · simulation");
    return s.attendant?text("Virtual attendant has control"):text("Virtual passenger has control");
}

}
