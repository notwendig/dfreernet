#include "registered_resume.hpp"
#include <QJsonArray>
#include <QtTest>
using namespace attendant;
class ResumeTest : public QObject {
    Q_OBJECT
    static QJsonObject profile() {
        return {{"identities",QJsonArray{"1122334455667788","0100000000000000","0200000000000000","0300000000000000","0400000000000000"}},
                {"resume_reference",QJsonObject{{"configuration",QJsonArray{"01000000","02000000"}},
                                              {"flags","12345678"},{"profiles","80000000"}}}};
    }
    static QStringList fingerprint(RegisteredResume &r) {
        r.begin(0);
        r.receive(0x00e,QByteArray::fromHex("0100000000000000"),0);
        r.receive(0x03c30f0f,QByteArray(7,char(0x87)),10);
        r.receive(0x00e,QByteArray::fromHex("0100000000000000"),20);
        return r.receive(0x03c30f0f,QByteArray(7,char(0x87)),30);
    }
    static QStringList identities(RegisteredResume &r) {
        QStringList frames;
        for(int node=1;node<=4;++node) {
            QByteArray identity(8,0);identity[0]=char(node);
            for(int part=0;part<2;++part) {
                QByteArray reply=QByteArray::fromHex(part?"2fc90000":"2fc80000");
                reply+=identity.mid(part*4,4);
                frames+=r.receive(0x790+node,reply,40+node*4+part);
            }
        }
        return frames;
    }
    static QStringList configuration(RegisteredResume &r) {
        QStringList frames;int time=100;
        for(const auto *value:{"4080000000000000","2080000010000000","4081000000000000",
                              "208f000001000000","4081000000000000","208f000002000000",
                              "4080000000000000"})frames+=r.receive(0x793,QByteArray::fromHex(value),time++);
        return frames;
    }
private slots:
    void verifiedExistingAssignmentActivatesWithoutRegistrationOrTakeover() {
        RegisteredResume r;QVERIFY(r.configure(profile()));QVERIFY(r.configured());
        auto frames=fingerprint(r);QCOMPARE(frames,QStringList{"78f#41c8000000000000"});
        frames+=identities(r);QCOMPARE(r.state(),RegisteredResume::State::Configuration);
        frames+=configuration(r);QCOMPARE(r.state(),RegisteredResume::State::Flags);
        frames+=r.receive(0x793,QByteArray::fromHex("2040000012345678"),200);
        QCOMPARE(r.state(),RegisteredResume::State::Profiles);
        frames+=r.receive(0x793,QByteArray::fromHex("2050000080000000"),201);
        QVERIFY(r.ready());QVERIFY(r.error().isEmpty());QCOMPARE(frames.size(),17);
        for(const auto &frame:frames) {
            QVERIFY(frame.startsWith("780#") || frame.startsWith("78f#")); // only POP, no discovery or motion
            const auto body=QByteArray::fromHex(frame.mid(4).toLatin1());
            QVERIFY((quint8(body[0])&0xf0)==0x40 || quint8(body[0])==0x23);
        }
        QVERIFY(r.tick(100000).isEmpty());QVERIFY(r.ready());
        r.reset();QVERIFY(!r.ready());QVERIFY(!r.started());
    }
    void fingerprintMustMatchAndRepeat() {
        RegisteredResume r;QVERIFY(r.configure(profile()));r.begin(0);
        QVERIFY(r.receive(0x00e,QByteArray::fromHex("9900000000000000"),10).isEmpty());
        QCOMPARE(r.state(),RegisteredResume::State::Failed);QVERIFY(!r.ready());
        r.reset();r.begin(0);r.receive(0x00e,QByteArray::fromHex("0100000000000000"),0);
        QVERIFY(r.receive(0x03c30f0f,QByteArray(7,char(0x87)),10).isEmpty());
        QVERIFY(r.tick(1500).isEmpty());QCOMPARE(r.state(),RegisteredResume::State::Failed);
        QVERIFY(r.receive(0x00e,QByteArray::fromHex("0100000000000000"),1600).isEmpty());
    }
    void registeredNodeIdentityMustMatch() {
        RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);
        QVERIFY(r.receive(0x791,QByteArray::fromHex("2fc8000099000000"),40).isEmpty());
        QCOMPARE(r.state(),RegisteredResume::State::Failed);
        QVERIFY(r.tick(10000).isEmpty()); // one attempt, no retry or registration fallback
    }
    void otherClientsAndPartialRepliesCannotAdvance() {
        RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);
        QVERIFY(r.receive(0x791,QByteArray::fromHex("20c8000001000000"),40).isEmpty());
        QVERIFY(r.receive(0x792,QByteArray::fromHex("2fc8000001000000"),41).isEmpty());
        QVERIFY(r.receive(0x791,QByteArray::fromHex("2fc80000010000"),42).isEmpty());
        QCOMPARE(r.state(),RegisteredResume::State::Identities);
        r.tick(530);QCOMPARE(r.state(),RegisteredResume::State::Failed);
    }
    void conflictingSlotZeroStopsBeforeActivation() {
        for(quint32 id:{0x780u,0x02000000u,0x040u,0x1fb00000u}) {
            RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);
            QVERIFY(r.receive(id,QByteArray(8,0),40).isEmpty());
            QCOMPARE(r.state(),RegisteredResume::State::Failed);
        }
    }
    void configTimeoutAndCancellationReleaseOnlyOurReadSession() {
        RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);identities(r);
        QCOMPARE(r.tick(1100),QStringList{"780#2380000000000000"});
        QVERIFY(r.tick(2000).isEmpty());QCOMPARE(r.state(),RegisteredResume::State::Failed);
        r.reset();fingerprint(r);identities(r);
        QCOMPARE(r.cancel(),QStringList{"780#2380000000000000"});
        QVERIFY(r.cancel().isEmpty());QVERIFY(!r.started());QVERIFY(!r.ready());
    }
    void rejectedOpenDoesNotReleaseAnotherSession() {
        RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);identities(r);
        QVERIFY(r.receive(0x793,QByteArray::fromHex("c080000000000000"),100).isEmpty());
        QCOMPARE(r.state(),RegisteredResume::State::Failed);QVERIFY(r.tick(2000).isEmpty());
    }
    void changedConfigurationAndAssignmentsAreRejected() {
        RegisteredResume r;QVERIFY(r.configure(profile()));fingerprint(r);identities(r);configuration(r);
        r.receive(0x793,QByteArray::fromHex("2040000012345678"),200);
        r.receive(0x793,QByteArray::fromHex("2050000001000000"),201);
        QCOMPARE(r.state(),RegisteredResume::State::Failed);QVERIFY(!r.ready());
        auto p=profile();auto ref=p.value("resume_reference").toObject();ref["profiles"]="01000000";p["resume_reference"]=ref;
        QVERIFY(!r.configure(p));QVERIFY(!r.configured());
        p=profile();ref=p.value("resume_reference").toObject();ref["flags"]="zzzzzzzz";p["resume_reference"]=ref;
        QVERIFY(!r.configure(p));QVERIFY(!r.configured());
        QVERIFY(r.configure(QJsonObject{}));QVERIFY(!r.configured()); // backward compatible, power-on only
    }
    void changedParameterSignatureOrFlagsCannotActivate() {
        auto p=profile();auto ref=p.value("resume_reference").toObject();
        ref["configuration"]=QJsonArray{"99000000","02000000"};p["resume_reference"]=ref;
        RegisteredResume r;QVERIFY(r.configure(p));fingerprint(r);identities(r);
        const auto frames=configuration(r);
        QCOMPARE(r.state(),RegisteredResume::State::Failed);QVERIFY(!r.ready());
        QVERIFY(!frames.contains("780#4340000000000000"));
        r.configure(profile());fingerprint(r);identities(r);configuration(r);
        r.receive(0x793,QByteArray::fromHex("2040000099999999"),200);
        QCOMPARE(r.state(),RegisteredResume::State::Failed);QVERIFY(!r.ready());
    }
};
QTEST_GUILESS_MAIN(ResumeTest)
#include "attendant_resume_test.moc"
