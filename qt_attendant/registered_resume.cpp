#include "registered_resume.hpp"
#include <QJsonArray>
#include <QRegularExpression>

namespace attendant {
bool RegisteredResume::configure(const QJsonObject &profile) {
    reset();configured_=false;identities_.clear();values_.clear();flags_.clear();profiles_.clear();
    if(!profile.contains("resume_reference"))return true; // older profiles require power-on
    const auto reference=profile.value("resume_reference").toObject();
    const auto identities=profile.value("identities").toArray();
    const auto values=reference.value("configuration").toArray();
    const QRegularExpression hex8("^[0-9a-fA-F]{8}$"),hex16("^[0-9a-fA-F]{16}$");
    if(identities.size()!=5 || values.size()!=2 ||
       !hex8.match(reference.value("flags").toString()).hasMatch() ||
       !hex8.match(reference.value("profiles").toString()).hasMatch())return false;
    for(int i=0;i<5;++i) {
        if(!hex16.match(identities[i].toString()).hasMatch())return false;
        identities_[i]=QByteArray::fromHex(identities[i].toString().toLatin1());
    }
    for(const auto &v:values) {
        if(!hex8.match(v.toString()).hasMatch())return false;
        values_.append(QByteArray::fromHex(v.toString().toLatin1()));
    }
    flags_=QByteArray::fromHex(reference.value("flags").toString().toLatin1());
    profiles_=QByteArray::fromHex(reference.value("profiles").toString().toLatin1());
    configured_=profiles_==QByteArray::fromHex("80000000"); // captured attendant profile 8
    return configured_;
}
void RegisteredResume::reset() {
    state_=State::Waiting;config_.reset();error_.clear();
    start_=deadline_=0;firstFingerprint_=-1;driverFrames_=heartbeats_=identityStep_=0;
    pageOpen_=openConfirmed_=false;
}
void RegisteredResume::begin(qint64 now) {
    if(!configured() || started())return;
    state_=State::Fingerprint;start_=now;deadline_=now+1500;
}
QStringList RegisteredResume::cancel() {
    const bool release=pageOpen_;
    reset();
    return release?QStringList{QStringLiteral("780#2380000000000000")}:QStringList{};
}
void RegisteredResume::fail(const QString &error) {
    if(!running() && !ready())return;
    state_=State::Failed;error_=error;pageOpen_=false;
}
QStringList RegisteredResume::abort(const QString &error) {
    const bool release=pageOpen_;
    fail(error);
    return release?QStringList{QStringLiteral("780#2380000000000000")}:QStringList{};
}
QString RegisteredResume::identityRequest() const {
    const int node=1+identityStep_/2,odi=0xc8+identityStep_%2;
    // Use the established identification-only programmer read addressing.
    // Configuration below still belongs exclusively to the BGM client 0.
    return QString("78f#%1%2%3").arg(0x40|node,2,16,QLatin1Char('0'))
        .arg(odi,2,16,QLatin1Char('0')).arg("000000000000");
}
QStringList RegisteredResume::tick(qint64 now) {
    if(!running())return {};
    if(now<start_ || now-start_>=15000 || now>=deadline_)
        return abort(QStringLiteral("Registered BGM verification timed out; restart the system with this app open"));
    return {};
}
QStringList RegisteredResume::receive(quint32 id,const QByteArray &data,qint64 now) {
    auto outgoing=tick(now);
    if(!running())return outgoing;
    if(id==0x780 || id==0x02000000 || id==0x040 || id==0x1fb00000) {
        // Local loopback is excluded by CanObserver. Another live slot-0 device
        // must not share this process's address, even with a matching serial.
        fail(QStringLiteral("Another device is using BGM slot 0"));return {};
    }
    if(state_==State::Fingerprint) {
        if(id==0x00e) {
            if(data!=identities_.value(1))return abort(QStringLiteral("Registered device assignment differs from the profile"));
            ++driverFrames_;
        }
        if(id==0x03c30f0f && !data.isEmpty() && data==QByteArray(data.size(),char(0x87)))++heartbeats_;
        if(id==0x00e || id==0x03c30f0f) {
            if(firstFingerprint_<0)firstFingerprint_=now;
            if(now-firstFingerprint_>300) {
                driverFrames_=heartbeats_=0;firstFingerprint_=now;
            }
        }
        if(driverFrames_<2 || heartbeats_<2)return {};
        state_=State::Identities;deadline_=now+500;
        return {identityRequest()};
    }
    if(state_==State::Identities) {
        const int node=1+identityStep_/2,odi=0xc8+identityStep_%2;
        if(id!=quint32(0x790+node) || data.size()!=8 || quint8(data[1])!=odi || data[2] || data[3])return {};
        if(quint8(data[0])==0xcf)return abort(QStringLiteral("Registered BGM identity read was rejected"));
        if(quint8(data[0])!=0x2f)return {};
        if(data.mid(4)!=identities_.value(node).mid((identityStep_%2)*4,4))
            return abort(QStringLiteral("Registered device assignment differs from the profile"));
        if(++identityStep_<8) {deadline_=now+500;return {identityRequest()};}
        state_=State::Configuration;pageOpen_=true;deadline_=now+1000;
        return {config_.begin(now)};
    }
    if(state_==State::Configuration) {
        if(id==0x793 && data==QByteArray::fromHex("4080000000000000"))openConfirmed_=true;
        if(!openConfirmed_ && id==0x793 && data.size()==8 && data.left(4)==QByteArray::fromHex("c0800000"))pageOpen_=false;
        const auto next=config_.receive(id,data,now);
        if(config_.state()==StartupConfig::State::Failed)return abort(config_.error());
        if(config_.state()==StartupConfig::State::Ready) {
            pageOpen_=false;
            if(config_.value(3)!=values_[0] || config_.value(5)!=values_[1])
                return abort(QStringLiteral("Stored BGM configuration differs from the profile"));
            state_=State::Flags;deadline_=now+500;
            return {QStringLiteral("780#4340000000000000")};
        }
        if(!next.isEmpty()) {deadline_=now+1000;return {next};}
        return {};
    }
    if(id!=0x793 || data.size()!=8 || data[2] || data[3])return {};
    const int odi=state_==State::Flags?0x40:0x50;
    if(quint8(data[1])!=odi)return {};
    if(quint8(data[0])==0xc0)return abort(QStringLiteral("Registered BGM configuration read was rejected"));
    if(quint8(data[0])!=0x20)return {};
    if(data.mid(4)!=(state_==State::Flags?flags_:profiles_))
        return abort(QStringLiteral("Stored BGM configuration differs from the profile"));
    if(state_==State::Flags) {
        state_=State::Profiles;deadline_=now+500;
        return {QStringLiteral("780#4350000000000000")};
    }
    state_=State::Ready;
    return {}; // no ready reset, enumeration, takeover or joystick output
}
}
