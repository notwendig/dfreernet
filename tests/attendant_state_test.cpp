#include "attendant_state.hpp"
#include "can_control.hpp"
#include "startup_config.hpp"
#include "module_enrollment.hpp"
#include <QJsonDocument>
#include <QRegularExpression>
#include "capture_playback.hpp"
#include <QFileInfo>
#include <QDir>
#include "attendant_controller.hpp"

#include <QJsonDocument>
#include <QtTest>
#include <limits>
#include <cstring>
#if defined(Q_OS_LINUX)
#include <linux/can.h>
#endif
#include <cmath>
using namespace attendant;
class AttendantTest : public QObject {
    Q_OBJECT
    static void confirmProfile(CanControl &control, bool attendant, qint64 time){
        control.receive(0x053,QByteArray::fromHex(attendant ? "10070002" : "10000002"),time);
        control.receive(0x053,QByteArray::fromHex(attendant ? "80070001" : "81000001"),time+1);
        const auto frames = control.takeOutgoing();
        const auto selection = QString::number((attendant ? 0x70 : 0) | control.mode(),16).rightJustified(2,'0');
        if (attendant)
            QCOMPARE(frames,QStringList{QString("060#00")+selection+"0000"});
        else
            QVERIFY(frames.isEmpty());
    }
private slots:
    void registrationOnlyDisablesDriving(){
        AttendantController controller;
        controller.registerOnDataFrame();
        controller.setRegistrationOnly(true);
        QVERIFY(!controller.controlsEnabled());
        controller.toggleDrivingControl();
        controller.requestControl();
        controller.setAxes(100,100);
        QVERIFY(!controller.attendantControl());
        QCOMPARE(controller.outputX(),0);
        QCOMPARE(controller.outputY(),0);
    }
    void profileAcknowledgementPrecedesModeAndJoystick(){
        CanControl control;
        QCOMPARE(control.request(true,0),QStringList{"050#00070000"});
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("10000002"),1));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80000080"),2));
        QVERIFY(!control.owned());
        QVERIFY(control.takeOutgoing().isEmpty());
        control.receive(0x053,QByteArray::fromHex("80070001"),3);
        QVERIFY(control.takeOutgoing().isEmpty());
        control.receive(0x053,QByteArray::fromHex("10070002"),4);
        control.receive(0x053,QByteArray::fromHex("80070001"),5);
        QCOMPARE(control.takeOutgoing(),QStringList{"060#00700000"});
        control.receive(0x063,QByteArray::fromHex("10000002"),6);
        control.receive(0x063,QByteArray::fromHex("80000080"),7);
        QVERIFY(control.owned());
        QCOMPARE(control.request(false,8),(QStringList{"02000000#0000","060#40700000"}));
        control.receive(0x063,QByteArray::fromHex("50000002"),8);
        control.receive(0x063,QByteArray::fromHex("80000010"),8);
        QCOMPARE(control.takeOutgoing(),QStringList{"050#00000000"});
        control.receive(0x053,QByteArray::fromHex("10000002"),9);
        control.receive(0x053,QByteArray::fromHex("81000001"),10);
        QVERIFY(control.takeOutgoing().isEmpty());
        control.receive(0x061,QByteArray::fromHex("00010000"),11);
        control.receive(0x063,QByteArray::fromHex("11010002"),12);
        QVERIFY(control.receive(0x063,QByteArray::fromHex("80010080"),13));
        QVERIFY(!control.owned());
        QVERIFY(control.joystick(100,100).isEmpty());
        QCOMPARE(control.request(true,14),QStringList{"050#00070000"});
    }
    void attendantOutputRemainsInProfileEight(){
        CanControl control;
        QCOMPARE(control.request(true,0),QStringList{"050#00070000"});
        confirmProfile(control,true,0);
        control.receive(0x063,QByteArray::fromHex("10000002"),1);
        control.receive(0x063,QByteArray::fromHex("80000080"),2);
        QVERIFY(control.owned());
        QVERIFY(!control.joystick(20,0).isEmpty());
        QVERIFY(control.receive(0x050,QByteArray::fromHex("00060000"),3));
        QVERIFY(!control.owned());
        QVERIFY(control.joystick(20,0).isEmpty());
        QVERIFY(control.speed(100).isEmpty());
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80000080"),4));
        control.request(true,10);
        QVERIFY(control.receive(0x061,QByteArray::fromHex("00000000"),11));
        QVERIFY(!control.pending());
        QVERIFY(control.joystick(20,0).isEmpty());
        AttendantController controller;
        controller.registerOnDataFrame();
        QCOMPARE(controller.profile(),8);
        controller.nextProfile();
        QCOMPARE(controller.profile(),8);
        QCOMPARE(controller.modeNumber(),2);
    }
    void liveEnrollmentBlockedAfterFault(){
        AttendantController controller;
        controller.registerOnDataFrame();
        QVERIFY(controller.controlsEnabled());
        controller.blockLiveEnrollment();
        QVERIFY(!controller.controlsEnabled());
        QVERIFY(!controller.canTxAllowed());
        controller.registerOnDataFrame();
        controller.toggleDrivingControl();
        QVERIFY(!controller.attendantControl());
        QVERIFY(!controller.canTxConnected());
    }
    void enrollmentRespondsAndTimesOut(){
        ModuleEnrollment enrollment;
        const auto profile = QJsonDocument::fromJson(R"({"identity":"1122334455667788","announcement_keys":"0102030405060708","other_high_words":[1,2,3,4]})").object();
        QVERIFY(enrollment.configure(profile));
        enrollment.requestInitialEnrollment();
        enrollment.boot(0);
        QVERIFY(enrollment.tick(199).isEmpty());
        QCOMPARE(enrollment.tick(200),QStringList{"1f000111#"});
        QCOMPARE(enrollment.receive(0x1f800010,{},300),QStringList{"1f908877#"});
        enrollment.receive(0x1f800011,{},300);
        enrollment.receive(0x1f800012,{},300);
        QCOMPARE(enrollment.receive(0x1f800013,{},301),QStringList{"1f932211#"});
        enrollment.receive(0x7b0,{},302);
        QVERIFY(enrollment.tick(350).isEmpty());
        for (unsigned i=1;i<=4;++i)enrollment.receive(0x1f930000u|i,{},400+i);
        QCOMPARE(enrollment.receive(0x1f800020,{},500),QStringList{"1fa00014#"});
        QCOMPARE(enrollment.receive(0x1f800030,{},550),QStringList{"1fb00000#1122334455667788"});
        enrollment.receive(0x1ec42040,QByteArray::fromHex("0103000000000000"),600);
        QVERIFY(enrollment.configurationRequested());
        QVERIFY(!enrollment.running());
        enrollment.boot(1000);
        enrollment.tick(7000);
        QVERIFY(!enrollment.error().isEmpty());
        QVERIFY(!enrollment.running());
    }
    void enrollmentMatchesPrivateStartupTrace(){
        QFile profileFile(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/../build").filePath("attendant-enrollment.json"));
        QFile trace(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/..").filePath("insys.txt"));
        if(!profileFile.open(QIODevice::ReadOnly)||!trace.open(QIODevice::ReadOnly))QSKIP("Private enrollment fixtures unavailable");
        ModuleEnrollment enrollment;
        const auto profile = QJsonDocument::fromJson(profileFile.readAll()).object();
        QVERIFY(enrollment.configure(profile));
        const auto identity = QByteArray::fromHex(profile.value("identity").toString().toLatin1());
        enrollment.requestInitialEnrollment();
        enrollment.boot(0);
        QStringList responses;
        const auto ownAnnouncement = QString("1f00%1%2#")
            .arg(QString::fromLatin1(QByteArray::fromHex(profile.value("announcement_keys").toString().toLatin1()).left(1).toHex()))
            .arg(QString::fromLatin1(identity.left(1).toHex()));
        const QRegularExpression syntax(R"(^\s*\(([0-9.]+)\)\s+can0\s+([0-9A-F]+)\s+\[([0-8])\]\s*(.*)$)");
        for(const auto &line:QString::fromUtf8(trace.readAll()).split('\n')){
            const auto match=syntax.match(line);
            QVERIFY(match.hasMatch());
            const auto time=qRound64(match.captured(1).toDouble()*1000);
            QByteArray data;
            const auto tail=match.captured(4);
            if(!tail.startsWith("remote request")){
                const auto fields=tail.split(QRegularExpression(R"(\s+)"),Qt::SkipEmptyParts);
                QByteArray hex;
                for(int i=0;i<match.captured(3).toInt();++i)hex+=fields[i].toLatin1();
                data=QByteArray::fromHex(hex);
            }
            const auto advertised = enrollment.tick(time);
            for (const auto &frame : advertised) QCOMPARE(frame, ownAnnouncement);
            responses+=advertised;
            responses+=enrollment.receive(match.captured(2).toUInt(nullptr,16),data,time);
            if(enrollment.configurationRequested())break;
        }
        QVERIFY2(enrollment.error().isEmpty(),qPrintable(enrollment.error()));
        QVERIFY(enrollment.configurationRequested());
        const auto highWord = quint8(identity[0]) | (quint16(quint8(identity[1])) << 8);
        QVERIFY(responses.contains(QString("1f93%1#").arg(highWord,4,16,QLatin1Char('0'))));
        QVERIFY(responses.contains("1fa00014#"));
        QVERIFY(responses.contains("1fb00000#" + identity.toHex()));
    }
    void registeredBootDoesNotRestartEnrollment(){
        const auto profile=QJsonDocument::fromJson(R"({"identity":"1122334455667788","announcement_keys":"0102030405060708","other_high_words":[1,2,3,4],"identities":["1122334455667788","0100000000000000","0200000000000000","0300000000000000","0400000000000000"],"startup_tables":[{"id":"1ec00800","data":"0000000000000000","ms":0},{"id":"1ec10800","data":"0000000000000000","ms":0}]})").object();
        ModuleEnrollment enrollment;
        QVERIFY(enrollment.configure(profile));
        enrollment.allowAutomaticEnrollment(false);
        enrollment.boot(0);
        // The JSM announces the pre-join network before our own announcement.
        enrollment.receive(0x7b0,{},20);
        for(unsigned slot=0;slot<4;++slot)
            for(unsigned part=0;part<8;++part)
                enrollment.receive(0x1f000000u|(part<<20)|(slot<<16)|((0x0c+part)<<8)|slot,{},40);
        QVERIFY(enrollment.error().isEmpty());
        QVERIFY(!enrollment.complete());
        QVERIFY(enrollment.tick(199).isEmpty());
        const auto burst=enrollment.tick(200);
        QCOMPARE(burst.size(),8);
        QCOMPARE(burst.first(),QString("1f000111#"));
        QCOMPARE(burst.last(),QString("1f700888#"));
        QVERIFY(enrollment.tick(300).isEmpty());
        // An overlapping old burst must not reject the new discovery phase.
        for(unsigned part=0;part<8;++part)
            enrollment.receive(0x1f010000u|(part<<20)|((0x0c+part)<<8)|1,{},310);
        QVERIFY(enrollment.error().isEmpty());
        enrollment.receive(0x7b0,{},400);
        QVERIFY(!enrollment.complete());
        for(unsigned slot=1;slot<=4;++slot)
            for(unsigned part=0;part<8;++part)
                QVERIFY(enrollment.receive(0x1f000000u|(part<<20)|(slot<<16)|((part+1)<<8)|(part==0?slot:0),{},500).isEmpty());
        QVERIFY(enrollment.complete());
        QVERIFY(!enrollment.configurationRequested());
        QVERIFY(enrollment.running());
        QVERIFY(enrollment.tick(520).isEmpty());
        QVERIFY(!enrollment.configurationRequested());
        // Other traffic during the settling interval must not postpone it.
        enrollment.receive(0x00e,QByteArray(8,0),520);
        QVERIFY(enrollment.tick(521).isEmpty());
        QVERIFY(enrollment.configurationRequested());
        QVERIFY(!enrollment.running());
        QVERIFY(enrollment.tick(1000).isEmpty());
        // Power-down/reset must not switch back to candidate registration.
        enrollment.reset();
        enrollment.boot(2000);
        QCOMPARE(enrollment.tick(2200).size(),8);
        // An unexpected request never silently upgrades into initial enrollment.
        enrollment.receive(0x1f800010,{},2300);
        QVERIFY(!enrollment.error().isEmpty());
        QVERIFY(!enrollment.complete());
        QVERIFY(enrollment.tick(2400).isEmpty());
        // Explicit initial enrollment is consumed by one boot only.
        enrollment.requestInitialEnrollment();
        enrollment.boot(3000);
        QCOMPARE(enrollment.tick(3200).size(),1);
        enrollment.reset();
        enrollment.boot(4000);
        QCOMPARE(enrollment.tick(4200).size(),8);
    }
    void automaticStartupRequiresExplicitEnumerationAndMatchingPeers(){
        const auto profile=QJsonDocument::fromJson(R"({"identity":"1122334455667788","announcement_keys":"0102030405060708","other_high_words":[1,2,3,4],"identities":["1122334455667788","0100000000000000","0200000000000000","0300000000000000","0400000000000000"],"startup_tables":[{"id":"1ec00800","data":"0000000000000000","ms":0},{"id":"1ec10800","data":"0000000000000000","ms":0}]})").object();
        ModuleEnrollment enrollment;
        QVERIFY(enrollment.configure(profile));
        QVERIFY(enrollment.liveProfileReady());
        enrollment.boot(0);
        QVERIFY(enrollment.receive(0x1f800010,{},100).isEmpty());
        QCOMPARE(enrollment.tick(200).size(),8);
        QCOMPARE(enrollment.receive(0x1f800010,{},300),QStringList{"1f908877#"});
        for(unsigned part=1;part<4;++part) QVERIFY(!enrollment.receive(0x1f800010+part,{},310+part).isEmpty());
        for(unsigned word=1;word<=4;++word) enrollment.receive(0x1f930000u|word,{},400);
        QCOMPARE(enrollment.receive(0x1f800020,{},500),QStringList{"1fa00014#"});
        QCOMPARE(enrollment.receive(0x1f800030,{},550),QStringList{"1fb00000#1122334455667788"});
        for(unsigned slot=1;slot<=4;++slot)
            enrollment.receive(0x1fb00000u|slot,QByteArray::fromHex(profile.value("identities").toArray()[int(slot)].toString().toLatin1()),600);
        QCOMPARE(enrollment.tick(660),QStringList{"1ec00800#0000000000000000"});
        enrollment.tick(3000);
        QVERIFY(!enrollment.error().isEmpty());
        QVERIFY(enrollment.tick(4000).isEmpty()); // no automatic retry loop
        enrollment.boot(5000);
        QCOMPARE(enrollment.tick(5200).size(),8); // next boot tries normal startup
        enrollment.receive(0x1f800011,{},5300);
        QVERIFY(!enrollment.error().isEmpty()); // no partial enumeration
        enrollment.boot(6000);
        enrollment.tick(6200);
        enrollment.receive(0x1f800010,{},6300);
        QVERIFY(enrollment.receive(0x1f800020,{},6400).isEmpty());
        QVERIFY(!enrollment.error().isEmpty());
        auto damaged=profile;
        damaged["identity"]="1122334455667788zz";
        QVERIFY(!enrollment.configure(damaged));
        QVERIFY(!enrollment.configured());
        QVERIFY(enrollment.configure(profile));
        enrollment.boot(7000);
        enrollment.tick(7200);
        enrollment.receive(0x1f000111,{},7250);
        QVERIFY(!enrollment.error().isEmpty());
        QVERIFY(!enrollment.running());
        damaged=profile;
        auto identities=damaged.value("identities").toArray();
        identities[2]="0500000000000000";
        damaged["identities"]=identities;
        QVERIFY(!enrollment.configure(damaged));
    }
    void controllerNeedsFreshInputUpdates(){
        AttendantController controller;
        controller.registerOnDataFrame();
        controller.toggleDrivingControl();
        controller.setAxes(100,0);
        QCOMPARE(controller.outputX(),100);
        for(int i=0;i<10;++i) {
            QTest::qWait(30);
            controller.setAxes(100,0);
        }
        QCOMPARE(controller.outputX(),100);
        QTest::qWait(AttendantState::timeoutMs+50);
        QCOMPARE(controller.outputX(),0);
        QVERIFY(!controller.attendantControl());
        controller.setAxes(100,0);
        QCOMPARE(controller.outputX(),0); // stale takeover cannot re-enable motion
        controller.setAxes(0,0);
        controller.toggleDrivingControl();
        controller.setAxes(100,0);
        QCOMPARE(controller.outputX(),100);
    }
    void powerDownStaysLatchedUntilNewPowerOn(){
        AttendantController controller;
        controller.registerOnDataFrame();
        controller.toggleDrivingControl();
        controller.setAxes(100,0);
        QCOMPARE(controller.outputX(),100);
        controller.systemPowerOffRequested();
        QCOMPARE(controller.outputX(),0);
        QVERIFY(!controller.powered());
        QVERIFY(!controller.controlsEnabled());
        controller.registerOnDataFrame();
        QVERIFY(!controller.powered());
        // Invalid interface name guarantees that the test cannot open CAN.
        controller.setCanInterface("offline-test-no-interface-allowed");
        controller.allowCanTx(true);
        auto *observer=controller.findChild<CanObserver *>();
        QVERIFY(observer);
        observer->dataFrame(0x004,{});
        observer->protocolFrame(0x1f800010,{});
        QVERIFY(!controller.powered());
        QCOMPARE(controller.canTxCount(),QString("0"));
        observer->dataFrame(0x00c,{});
        QVERIFY(controller.powered());
        QVERIFY(!controller.controlsEnabled());
        QCOMPARE(controller.canTxCount(),QString("0"));
        QVERIFY(controller.requestClose());
        QVERIFY(!controller.canTxConnected());
    }
    void registeredBootMatchesPrivateOriginalTrace(){
        const QDir root(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/..");
        QFile profile(root.filePath("build/attendant-enrollment-tables.json"));
        QFile trace(root.filePath("build/attendant-captures/startup-QifPV1oS.log"));
        if(!profile.open(QIODevice::ReadOnly)||!trace.open(QIODevice::ReadOnly)) QSKIP("Private registered-start fixtures unavailable");
        ModuleEnrollment enrollment;
        QVERIFY(enrollment.configure(QJsonDocument::fromJson(profile.readAll()).object()));
        enrollment.boot(0);
        QStringList emitted;
        const QRegularExpression syntax(R"(^\(([0-9.]+)\) can0 ([0-9A-F]+)#(.*)$)");
        double origin=-1;
        for(const auto &line:QString::fromUtf8(trace.readAll()).split('\n')) {
            const auto match=syntax.match(line);
            if(!match.hasMatch())continue;
            const auto stamp=match.captured(1).toDouble();
            if(origin<0)origin=stamp;
            const auto time=qRound64((stamp-origin)*1000);
            emitted+=enrollment.tick(time);
            const auto id=match.captured(2).toUInt(nullptr,16);
            // Local slot-0 burst is emitted by this emulator, not bus RX.
            if((id&0xff0f0000u)!=0x1f000000u)
                enrollment.receive(id,QByteArray::fromHex(match.captured(3).toLatin1()),time);
            if(enrollment.configurationRequested())break;
        }
        QVERIFY2(enrollment.error().isEmpty(),qPrintable(enrollment.error()));
        QVERIFY(enrollment.complete());
        QVERIFY(enrollment.configurationRequested());
        QCOMPARE(emitted.size(),8);
    }
    void startupTablesRequirePeerProgress(){
        QJsonArray records;
        for (unsigned stage : {0x0800u,0x0840u}) {
            for (unsigned slot=0;slot<5;++slot) {
                records.append(QJsonObject{{"id",QString::number(0x1ec00000u|(slot<<16)|stage,16)},
                                           {"data","0000000000000000"},{"ms",int(stage)}});
            }
        }
        StartupTables tables;
        QVERIFY(tables.configure(records));
        tables.begin(0);
        QVERIFY(tables.tick(59).isEmpty());
        QCOMPARE(tables.tick(60),QStringList{"1ec00800#0000000000000000"});
        QVERIFY(tables.tick(100).isEmpty());
        // Three peers are insufficient; no next-stage transmission is allowed.
        for(unsigned slot=1;slot<4;++slot) tables.receive(0x1ec00800u|(slot<<16),QByteArray(8,0));
        QVERIFY(tables.tick(200).isEmpty());
        tables.receive(0x1ec40800,QByteArray(8,0));
        QVERIFY(tables.tick(201).isEmpty());
        QCOMPARE(tables.tick(211),QStringList{"1ec00840#0000000000000000"});
        QVERIFY(!tables.complete());
        tables.tick(2000);
        QVERIFY(!tables.error().isEmpty());
        QVERIFY(!tables.complete());
        QVERIFY(tables.tick(2100).isEmpty());
    }
    void startupTablesHaveAnOverallDeadline(){
        QJsonArray records;
        for(unsigned stage=0;stage<40;++stage) {
            const auto low=0x0800u+stage*0x20u;
            for(unsigned slot=0;slot<2;++slot)
                records.append(QJsonObject{{"id",QString::number(0x1ec00000u|(slot<<16)|low,16)},
                                           {"data","0000000000000000"},{"ms",0}});
        }
        StartupTables tables;
        QVERIFY(tables.configure(records));
        tables.begin(0);
        for(unsigned stage=0;stage<40;++stage)
            tables.receive(0x1ec10800u+stage*0x20u,QByteArray(8,0));
        for(int time=1000;time<30000;time+=1000) {
            QVERIFY(!tables.tick(time).isEmpty());
            QVERIFY(tables.error().isEmpty());
        }
        QVERIFY(tables.tick(30000).isEmpty());
        QVERIFY(!tables.complete());
        QVERIFY(!tables.error().isEmpty());
    }
    void profileTableReplyWaitsForMatchingPmRecord(){
        QJsonArray records{
            QJsonObject{{"id","1ec33060"},{"data","0000018000400100"},{"ms",0}},
            QJsonObject{{"id","1ec03061"},{"data","0400018000400100"},{"ms",20}},
            QJsonObject{{"id","1ec33060"},{"data","0001028000400200"},{"ms",240}},
            QJsonObject{{"id","1ec03061"},{"data","0401028000400200"},{"ms",260}}};
        StartupTables tables;
        QVERIFY(tables.configure(records));
        tables.begin(0);
        QVERIFY(tables.tick(100).isEmpty());
        // A different slot or record cannot trigger a response.
        tables.receive(0x1ec23060,QByteArray::fromHex("0000018000400100"));
        tables.receive(0x1ec33060,QByteArray::fromHex("0000018000400200"));
        QVERIFY(tables.tick(500).isEmpty());
        tables.receive(0x1ec33060,QByteArray::fromHex("0000018000400100"));
        QCOMPARE(tables.tick(510),QStringList{"1ec03061#0400018000400100"});
        // Neither elapsed time nor duplicate first records trigger the next reply.
        tables.receive(0x1ec33060,QByteArray::fromHex("0000018000400100"));
        QVERIFY(tables.tick(900).isEmpty());
        tables.receive(0x1ec33060,QByteArray::fromHex("0001028000400200"));
        QCOMPARE(tables.tick(910),QStringList{"1ec03061#0401028000400200"});
        QVERIFY(tables.complete());
        QVERIFY(tables.tick(920).isEmpty());
    }
    void enrollmentStartsTablesAfterFullRoster(){
        QFile profileFile(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/../build").filePath("attendant-enrollment-tables.json"));
        if (!profileFile.open(QIODevice::ReadOnly)) QSKIP("Private startup tables unavailable");
        const auto profile=QJsonDocument::fromJson(profileFile.readAll()).object();
        ModuleEnrollment enrollment;
        QVERIFY(enrollment.configure(profile));
        enrollment.requestInitialEnrollment();
        enrollment.boot(0);
        enrollment.receive(0x7b0,{},200);
        for(const auto &word:profile.value("other_high_words").toArray())
            enrollment.receive(0x1f930000u|unsigned(word.toInt()),{},300);
        for(unsigned part=0;part<4;++part) enrollment.receive(0x1f800010+part,{},350);
        QVERIFY(!enrollment.receive(0x1f800020,{},400).isEmpty());
        QVERIFY(!enrollment.receive(0x1f800030,{},500).isEmpty());
        const auto identities=profile.value("identities").toArray();
        for(unsigned slot=1;slot<4;++slot)
            enrollment.receive(0x1fb00000u|slot,QByteArray::fromHex(identities[int(slot)].toString().toLatin1()),510);
        QVERIFY(enrollment.tick(590).isEmpty());
        enrollment.receive(0x1fb00004,QByteArray::fromHex(identities[4].toString().toLatin1()),600);
        QVERIFY(enrollment.tick(659).isEmpty());
        QCOMPARE(enrollment.tick(660),QStringList{"1ec00800#0000000000000000"});
        QVERIFY(!enrollment.complete());
        enrollment.tick(2300);
        QVERIFY(!enrollment.error().isEmpty());
        QVERIFY(!enrollment.running());
        QVERIFY(!enrollment.complete());
    }
    void startupTablesMatchPrivateTrace(){
        QFile profileFile(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/../build").filePath("attendant-enrollment-tables.json"));
        if (!profileFile.open(QIODevice::ReadOnly)) QSKIP("Private startup tables unavailable");
        const auto records=QJsonDocument::fromJson(profileFile.readAll()).object().value("startup_tables").toArray();
        StartupTables tables;
        QVERIFY(tables.configure(records));
        QStringList expected,actual;
        for(const auto &entry:records) {
            const auto row=entry.toObject();
            const auto id=row.value("id").toString().toUInt(nullptr,16);
            if (((id>>16)&15)==0)
                expected.append(QString("%1#%2").arg(id,8,16,QLatin1Char('0')).arg(row.value("data").toString()));
        }
        tables.begin(0);
        int next=0;
        const auto origin=records.first().toObject().value("ms").toInt();
        for(int time=0;time<20000&&!tables.complete()&&tables.error().isEmpty();time+=10) {
            while(next<records.size()&&records[next].toObject().value("ms").toInt()-origin+60<=time) {
                const auto row=records[next++].toObject();
                const auto id=row.value("id").toString().toUInt(nullptr,16);
                if (((id>>16)&15)!=0)
                    tables.receive(id,QByteArray::fromHex(row.value("data").toString().toLatin1()));
            }
            actual+=tables.tick(time);
        }
        QVERIFY2(tables.error().isEmpty(),qPrintable(tables.error()));
        QVERIFY(tables.complete());
        QCOMPARE(actual,expected);
        QCOMPARE(actual.size(),52);
    }
    void readinessNeedsBothReadsAndAllPeers(){
        StartupReady ready;
        QCOMPARE(ready.begin(0),QStringList{"780#4340000000000000"});
        QVERIFY(ready.receive(0x793,QByteArray::fromHex("21400000000081ff"),10).isEmpty());
        QCOMPARE(ready.receive(0x793,QByteArray::fromHex("20400000000081ff"),20),QStringList{"780#4350000000000000"});
        QVERIFY(!ready.ready());
        QCOMPARE(ready.receive(0x793,QByteArray::fromHex("2050000080000000"),30),QStringList{"040#00000000"});
        // Duplicate peers cannot satisfy the missing fourth device.
        for(unsigned slot=1;slot<4;++slot) {
            QVERIFY(ready.receive(0x040+slot,QByteArray(4,0),40).isEmpty());
            QVERIFY(ready.receive(0x040+slot,QByteArray::fromHex("80000000"),50).isEmpty());
        }
        QVERIFY(ready.receive(0x041,QByteArray(4,0),60).isEmpty());
        QVERIFY(ready.receive(0x045,QByteArray(4,0),60).isEmpty());
        QVERIFY(!ready.ready());
        QCOMPARE(ready.receive(0x044,QByteArray(4,0),70),QStringList{"040#80000000"});
        QVERIFY(!ready.ready());
        QVERIFY(ready.receive(0x044,QByteArray::fromHex("80000000"),80).isEmpty());
        QVERIFY(ready.ready());
        QVERIFY(ready.receive(0x044,QByteArray::fromHex("80000000"),90).isEmpty());
        ready.begin(1000);
        ready.tick(2000);
        QCOMPARE(ready.state(),StartupReady::State::Failed);
        QVERIFY(!ready.ready());
        ready.begin(3000);
        ready.receive(0x793,QByteArray::fromHex("c040000026000000"),3010);
        QCOMPARE(ready.state(),StartupReady::State::Failed);
        ready.begin(4000);
        ready.receive(0x793,QByteArray::fromHex("20400000000081ff"),4010);
        ready.receive(0x793,QByteArray::fromHex("2050000080000000"),4020);
        ready.tick(7020);
        QCOMPARE(ready.state(),StartupReady::State::Failed);
        ready.reset();
        QVERIFY(!ready.ready());
        QCOMPARE(ready.state(),StartupReady::State::Waiting);
    }
    void readinessMatchesOriginalRestart(){
        QFile trace(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/..").filePath("build/attendant-captures/startup-QifPV1oS.log"));
        if(!trace.open(QIODevice::ReadOnly)) QSKIP("Private normal-restart fixture unavailable");
        StartupReady ready;
        QStringList frames=ready.begin(0);
        const QRegularExpression syntax(R"(^\(([0-9.]+)\) can0 ([0-9A-F]+)#(.*)$)");
        double origin=-1;
        for(const auto &line:QString::fromUtf8(trace.readAll()).split('\n')) {
            const auto match=syntax.match(line);
            if(!match.hasMatch())continue;
            const auto stamp=match.captured(1).toDouble();
            if(origin<0)origin=stamp;
            const auto id=match.captured(2).toUInt(nullptr,16);
            if(id!=0x040)frames+=ready.receive(id,QByteArray::fromHex(match.captured(3).toLatin1()),qRound64((stamp-origin)*1000));
            if(ready.ready())break;
        }
        QVERIFY2(ready.ready(),qPrintable(ready.error()));
        QCOMPARE(frames,(QStringList{"780#4340000000000000","780#4350000000000000","040#00000000","040#80000000"}));
    }
    void startupConfigurationExchange(){
        StartupConfig config;
        QCOMPARE(config.begin(0), QString("780#2380000010000000"));
        QVERIFY(config.receive(0x793,QByteArray::fromHex("4180000000000000"),10).isEmpty());
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("4080000000000000"),11),QString("780#4380000000000000"));
        // A different selected page must not be accepted.
        QVERIFY(config.receive(0x793,QByteArray::fromHex("2080000011000000"),12).isEmpty());
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("2080000010000000"),13),QString("780#2381000006000100"));
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("4081000000000000"),14),QString("780#438f000000000000"));
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("208f000049350000"),15),QString("780#2381000002000000"));
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("4081000000000000"),16),QString("780#438f000000000000"));
        QCOMPARE(config.receive(0x793,QByteArray::fromHex("208f0000b3200000"),17),QString("780#2380000000000000"));
        QVERIFY(config.state() == StartupConfig::State::Loading);
        QVERIFY(config.receive(0x793,QByteArray::fromHex("4080000000000000"),18).isEmpty());
        QVERIFY(config.state() == StartupConfig::State::Ready);
        QCOMPARE(config.value(3),QByteArray::fromHex("49350000"));
        QCOMPARE(config.value(5),QByteArray::fromHex("b3200000"));
        config.reset();
        QCOMPARE(config.begin(100),QString("780#2380000010000000"));
        config.tick(1100);
        QVERIFY(config.state() == StartupConfig::State::Failed);
        QVERIFY(!config.error().isEmpty());
        QVERIFY(config.receive(0x793,QByteArray::fromHex("4080000000000000"),1101).isEmpty());
        config.begin(1200);
        config.receive(0x793,QByteArray::fromHex("c080000001000000"),1201);
        QVERIFY(config.state() == StartupConfig::State::Failed);
    }
    void startupConfigurationMatchesPrivateTrace(){
        const QDir root(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/..");
        QFile file(root.filePath("insys.txt"));
        if(!file.open(QIODevice::ReadOnly|QIODevice::Text))QSKIP("Private startup capture unavailable");
        StartupConfig config;
        auto pending=config.begin(1600);
        int requests=0;
        const QRegularExpression syntax(R"(^\s*\(([0-9.]+)\)\s+can0\s+(780|793)\s+\[8\]\s+((?:[0-9A-F]{2}\s+){7}[0-9A-F]{2}))");
        for (const auto &line : QString::fromUtf8(file.readAll()).split('\n')) {
            const auto match=syntax.match(line);
            if(!match.hasMatch())continue;
            auto data=match.captured(3);
            data.remove(QRegularExpression(R"(\s+)"));
            if(match.captured(2)=="780"){
                QCOMPARE(pending,QString("780#")+data.toLower());
                ++requests;
            } else {
                const auto next=config.receive(0x793,QByteArray::fromHex(data.toLatin1()),qRound64(match.captured(1).toDouble()*1000));
                if(!next.isEmpty())pending=next;
            }
        }
        QCOMPARE(requests,7);
        QVERIFY(config.state()==StartupConfig::State::Ready);
    }
    void returnWaitsForSuspendThenPassengerMode(){
        CanControl control;
        control.request(true,0);
        confirmProfile(control,true,10);
        control.receive(0x063,QByteArray::fromHex("10000002"),20);
        QVERIFY(control.receive(0x063,QByteArray::fromHex("80000080"),21));
        QCOMPARE(control.request(false,100),(QStringList{"02000000#0000","060#40700000"}));
        QVERIFY(!control.owned());
        QVERIFY(control.returning());
        QVERIFY(control.takeOutgoing().isEmpty());
        // Early profile and idle-mode replies cannot complete or advance return.
        control.receive(0x053,QByteArray::fromHex("10000002"),101);
        control.receive(0x053,QByteArray::fromHex("81000001"),102);
        control.receive(0x063,QByteArray::fromHex("80000010"),103);
        control.receive(0x062,QByteArray::fromHex("50000002"),104);
        control.receive(0x063,QByteArray::fromHex("50010002"),105);
        control.receive(0x063,QByteArray::fromHex("80000010"),106);
        QVERIFY(control.takeOutgoing().isEmpty());
        control.receive(0x063,QByteArray::fromHex("50000002"),120);
        control.receive(0x063,QByteArray::fromHex("60000000"),121);
        control.receive(0x063,QByteArray::fromHex("70000009"),122);
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80000010"),123));
        QCOMPARE(control.takeOutgoing(),QStringList{"050#00000000"});
        // Only suspension is confirmed so far. It must not count as return.
        QVERIFY(control.pending());
        QVERIFY(!control.owned());
        QVERIFY(control.joystick(10,10).isEmpty());
        QVERIFY(control.speed(20).isEmpty());
        control.receive(0x053,QByteArray::fromHex("10000000"),124);
        QVERIFY(control.pending());
        control.receive(0x053,QByteArray::fromHex("10000002"),125);
        control.receive(0x053,QByteArray::fromHex("81000001"),126);
        QVERIFY(control.takeOutgoing().isEmpty()); // no spoofed driver selection
        control.receive(0x061,QByteArray::fromHex("00010000"),127);
        control.receive(0x063,QByteArray::fromHex("11010002"),128);
        QVERIFY(control.receive(0x063,QByteArray::fromHex("80010080"),129));
        QVERIFY(!control.pending());
        QVERIFY(!control.owned());
        QVERIFY(!control.expired(2000));
    }
    void returnMatchesBothOriginalSequences(){
        QFile trace(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/..").filePath("ctrlsys.txt"));
        if(!trace.open(QIODevice::ReadOnly)) QSKIP("Private original-control fixture unavailable");
        const auto lines=QString::fromUtf8(trace.readAll()).split('\n');
        const QRegularExpression syntax(R"(^\s*\(([0-9.]+)\)\s+can0\s+([0-9A-F]+)\s+\[4\]\s+((?:[0-9A-F]{2}\s+){3}[0-9A-F]{2}))");
        for(int sequence=0;sequence<2;++sequence) {
            CanControl control;
            control.request(true,0);
            confirmProfile(control,true,1);
            control.receive(0x063,QByteArray::fromHex("10000002"),2);
            control.receive(0x063,QByteArray::fromHex("80000080"),3);
            const int mode=sequence==0?1:0;
            if(mode==1) {
                control.changeMode(4);
                control.receive(0x063,QByteArray::fromHex("50000002"),4);
                control.receive(0x063,QByteArray::fromHex("80000010"),4);
                QCOMPARE(control.takeOutgoing(),QStringList{"060#00710000"});
                control.receive(0x063,QByteArray::fromHex("10010002"),5);
                control.receive(0x063,QByteArray::fromHex("80010080"),6);
            }
            QVERIFY(control.owned());
            const qint64 origin=sequence==0?75917:201334;
            QCOMPARE(control.request(false,origin),
                     (QStringList{"02000000#0000",mode==1?"060#40710000":"060#40700000"}));
            int profileRequests=0;
            for(const auto &line:lines) {
                const auto match=syntax.match(line);
                if(!match.hasMatch())continue;
                const auto time=qRound64(match.captured(1).toDouble()*1000);
                if(time<origin||time>origin+500)continue;
                const auto id=match.captured(2).toUInt(nullptr,16);
                const auto data=QByteArray::fromHex(match.captured(3).toLatin1());
                if(id==0x050) {
                    QCOMPARE(control.takeOutgoing(),QStringList{"050#00000000"});
                    ++profileRequests;
                } else if(id!=0x060) control.receive(id,data,time);
            }
            QCOMPARE(profileRequests,1);
            QVERIFY(!control.pending());
            QVERIFY(!control.owned());
            QVERIFY(control.takeOutgoing().isEmpty());
        }
    }
    void speedCycleIncludesMinimumWithoutScalingAxes(){
        AttendantController controller;
        controller.registerOnDataFrame();
        controller.toggleDrivingControl();
        QVERIFY(controller.attendantControl());
        QCOMPARE(controller.inputLimit(),20);
        for(int expected:{40,60,80,100,0,20}) {
            controller.setAxes(0,0);
            controller.nextSpeed();
            QCOMPARE(controller.inputLimit(),expected);
            QVERIFY(controller.attendantControl());
            controller.setAxes(100,0);
            QCOMPARE(controller.outputX(),100);
        }
        controller.setAxes(0,0);
        controller.setInputLimit(-1);
        QCOMPARE(controller.inputLimit(),0);
        controller.nextSpeed();
        QCOMPARE(controller.inputLimit(),20);
    }
    void joystickTravelIsIndependentOfDriveSpeed(){
        for(int speed:{0,20,40,100}) {
            AttendantState state;
            Input input;
            input.limit=speed;
            input.attendant=true;
            input.request=1;
            state.beginSession(0);
            state.apply(input,1);
            input.hold=true;
            state.apply(input,2);
            input.x=100;
            state.apply(input,3);
            QCOMPARE(state.state().x,100);
            QCOMPARE(state.state().limit,speed);
            input.x=-100;
            state.apply(input,4);
            QCOMPARE(state.state().x,-100);
            input.x=input.y=100;
            state.apply(input,5);
            QCOMPARE(state.state().x,71);
            QCOMPARE(state.state().y,71);
            // Full travel still requires a centered, released takeover.
            state.stop();
            input.request=2;
            state.apply(input,6);
            QCOMPARE(state.state().x,0);
            QVERIFY(!state.state().attendant);
            input.x=input.y=0;
            input.hold=false;
            input.request=3;
            state.apply(input,7);
            input.hold=true;
            state.apply(input,8);
            input.y=-100;
            state.apply(input,9);
            QCOMPARE(state.state().y,-100);
            state.tick(259);
            QCOMPARE(state.state().y,0);
            QVERIFY(!state.state().held);
            input.attendant=false;
            input.hold=false;
            input.request=4;
            state.apply(input,260);
            QCOMPARE(state.state().y,0);
        }
    }
    void modeSwitchWaitsForSuspendAndCommitsOnlyOnConfirmation(){
        CanControl control;
        control.request(true,0);
        confirmProfile(control,true,1);
        control.receive(0x063,QByteArray::fromHex("10000002"),2);
        control.receive(0x063,QByteArray::fromHex("80000080"),3);
        for(int oldMode=0;oldMode<=1;++oldMode) {
            const qint64 time=100+oldMode*100;
            QCOMPARE(control.mode(),oldMode);
            const auto suspend=control.changeMode(time);
            QCOMPARE(suspend,(QStringList{"02000000#0000",oldMode==0?"060#40700000":"060#40710000"}));
            QVERIFY(!control.owned());
            QCOMPARE(control.mode(),oldMode);
            QVERIFY(control.changeMode(time+1).isEmpty());
            QVERIFY(control.takeOutgoing().isEmpty());
            // Early new-mode ACKs and the wrong old-mode suspend ACK are ignored.
            const auto newByte=oldMode==0?"01":"00";
            const auto oldByte=oldMode==0?"00":"01";
            control.receive(0x063,QByteArray::fromHex(QByteArray("10")+newByte+"0002"),time+2);
            control.receive(0x063,QByteArray::fromHex(QByteArray("80")+newByte+"0080"),time+3);
            control.receive(0x063,QByteArray::fromHex(QByteArray("50")+newByte+"0002"),time+4);
            control.receive(0x063,QByteArray::fromHex(QByteArray("80")+oldByte+"0010"),time+5);
            QVERIFY(control.takeOutgoing().isEmpty());
            QVERIFY(!control.owned());
            control.receive(0x063,QByteArray::fromHex(QByteArray("50")+oldByte+"0002"),time+6);
            control.receive(0x063,QByteArray::fromHex(QByteArray("80")+oldByte+"0010"),time+7);
            QCOMPARE(control.takeOutgoing(),QStringList{oldMode==0?"060#00710000":"060#00700000"});
            QCOMPARE(control.mode(),oldMode);
            QVERIFY(control.joystick(10,10).isEmpty());
            QVERIFY(control.speed(50).isEmpty());
            control.receive(0x063,QByteArray::fromHex(QByteArray("10")+newByte+"0002"),time+8);
            QVERIFY(control.receive(0x063,QByteArray::fromHex(QByteArray("80")+newByte+"0080"),time+9));
            QCOMPARE(control.mode(),oldMode^1);
            QVERIFY(control.owned());
        }
        control.changeMode(400);
        QVERIFY(control.expired(1400));
        control.reset();
        QCOMPARE(control.mode(),0); // no unconfirmed mode is remembered
        control.request(true,1500);
        confirmProfile(control,true,1501);
    }
    void canControlRequiresMatchingConfirmation(){
        CanControl control;
        QVERIFY(control.joystick(100,100).isEmpty());
        QCOMPARE(control.request(true,0), QStringList{"050#00070000"});
        confirmProfile(control,true,0);
        QVERIFY(control.pending());
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80000080"),10));
        QVERIFY(control.joystick(100,100).isEmpty());
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("11000002"),11));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80000080"),12));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("10000002"),13));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80010080"),14));
        QVERIFY(control.receive(0x063,QByteArray::fromHex("80000080"),15));
        QVERIFY(control.owned());
        QCOMPARE(control.joystick(-20,40),QString("02000000#ec28"));
        QCOMPARE(control.speed(20),QStringList{"0A040000#14"});
        QCOMPARE(control.speed(0),QStringList{"0A040000#00"});
        QCOMPARE(control.joystick(100,0),QString("02000000#6400"));
        const auto mode = control.changeMode(20);
        QCOMPARE(mode, (QStringList{"02000000#0000","060#40700000"}));
        QVERIFY(control.joystick(1,1).isEmpty());
        control.receive(0x063,QByteArray::fromHex("50000002"),21);
        control.receive(0x063,QByteArray::fromHex("80000010"),22);
        QCOMPARE(control.takeOutgoing(),QStringList{"060#00710000"});
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("10010002"),30));
        QVERIFY(control.receive(0x063,QByteArray::fromHex("80010080"),31));
        QCOMPARE(control.request(false,40), (QStringList{"02000000#0000","060#40710000"}));
        QVERIFY(control.expired(1040));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("11010002"),1041));
        QVERIFY(!control.receive(0x063,QByteArray::fromHex("80010080"),1042));
        control.reset();
        QVERIFY(!control.owned());
    }
    void recordedProtocolTransitions(){
        const QDir root(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() + "/..");
        if (!QFileInfo::exists(root.filePath("ctrlsys.txt")) ||
            !QFileInfo::exists(root.filePath("insys.txt")))
            QSKIP("Private startup/control captures unavailable");
        const auto startup = readCapture(root.filePath("insys.txt"));
        QVERIFY2(startup.error.isEmpty(), qPrintable(startup.error));
        bool enumerated = false;
        for (const auto &value : startup.events)
            enumerated |= !value.toMap().value("serial").toString().isEmpty();
        QVERIFY(enumerated);
        const auto control = readCapture(root.filePath("ctrlsys.txt"));
        QVERIFY2(control.error.isEmpty(), qPrintable(control.error));
        QCOMPARE(control.frames, 33970);
        for (const auto point : {qMakePair(16.0, 0), qMakePair(76.1, 1),
                                 qMakePair(127.9, 0), qMakePair(201.5, 1)}) {
            QVariantMap state;
            for (const auto &value : control.events) {
                if (value.toMap().value("time").toDouble() > point.first) break;
                state = value.toMap();
            }
            QCOMPARE(state.value("owner").toInt(), point.second);
        }
        QCOMPARE(control.events.last().toMap().value("phase").toString(), QString("Off"));
    }
    void simulatedIdentityOnStartup(){
        AttendantController controller;
        QVERIFY(controller.benchSerial().isEmpty());
        controller.registerOnDataFrame();
        const auto serial = controller.benchSerial();
        QVERIFY(serial.startsWith("SIM-"));
        controller.requestControl();
        controller.registerOnDataFrame();
        QVERIFY(controller.attendantControl());
        QCOMPARE(controller.benchSerial(), serial);
        controller.shutdown();
        QVERIFY(controller.benchSerial().isEmpty());
    }
    void manualCanFrameEncodingAndGuards(){
        TransmitFrame frame;
        QVERIFY(parse_transmit_frame("02000300#80ff",frame));
        QVERIFY(frame.extended);QVERIFY(!frame.remote);QCOMPARE(frame.id,0x02000300u);
        QCOMPARE(frame.data,QByteArray::fromHex("80ff"));
#if defined(Q_OS_LINUX)
        auto encoded=CanTransmitter::encode(frame);QCOMPARE(encoded.size(),qsizetype(sizeof(can_frame)));
        can_frame packet{};std::memcpy(&packet,encoded.constData(),sizeof(packet));
        QCOMPARE(packet.can_id,canid_t(CAN_EFF_FLAG|0x02000300u));QCOMPARE(packet.can_dlc,2);
        QCOMPARE(packet.data[0],0x80);QCOMPARE(packet.data[1],0xff);QCOMPARE(packet.data[7],0);
        QVERIFY(parse_transmit_frame("123#R8",frame));encoded=CanTransmitter::encode(frame);
        std::memcpy(&packet,encoded.constData(),sizeof(packet));
        QCOMPARE(packet.can_id,canid_t(CAN_RTR_FLAG|0x123));QCOMPARE(packet.can_dlc,8);
        QVERIFY(parse_transmit_frame("123#",frame));QVERIFY(!CanTransmitter::encode(frame).isEmpty());
#endif
        for(const auto &invalid:{"800#00","20000000#00","123#0","123#000000000000000000","123#R9","123##00","123#gg","123#00\n124#00"})
            QVERIFY(!parse_transmit_frame(invalid,frame));
        CanTransmitter tx;QVERIFY(!tx.send("123#00"));QCOMPARE(tx.count(),quint64(0));
        QVERIFY(!tx.open(QString(16,'x')));QVERIFY(!tx.connected());
        AttendantController controller;QVERIFY(!controller.canTxAllowed());
        controller.startCanTx();QVERIFY(!controller.canTxConnected());
        QCOMPARE(controller.canTxCount(),QString("0"));
    }
    void explicitNeutralTakeoverAndHold(){
        AttendantState m;Input in;QVERIFY(!m.apply(in,0));m.beginSession(0);
        in.attendant=true;in.request=1;in.x=50;QVERIFY(m.apply(in,1));QVERIFY(!m.state().attendant);
        in.x=0;QVERIFY(m.apply(in,2));QVERIFY(!m.state().attendant);
        in.request=2;QVERIFY(m.apply(in,3));QVERIFY(m.state().attendant);QVERIFY(!m.state().held);
        in.hold=true;m.apply(in,4);QVERIFY(m.state().held);
        in.x=100;m.apply(in,5);QCOMPARE(m.state().x,100);
        in.hold=false;m.apply(in,6);QCOMPARE(m.state().x,0);
        in.hold=true;m.apply(in,7);QVERIFY(!m.state().held);
        in.x=0;m.apply(in,8);QVERIFY(!m.state().held);
        in.hold=false;m.apply(in,9);in.hold=true;m.apply(in,10);QVERIFY(m.state().held);
        in.x=-100;m.apply(in,11);QCOMPARE(m.state().x,-100);
    }
    void watchdogNeverRestoresOldAuthority(){
        AttendantState m;m.beginSession(0);Input in;in.attendant=true;in.request=1;m.apply(in,0);in.hold=true;m.apply(in,1);in.y=100;m.apply(in,2);QCOMPARE(m.state().y,100);
        m.tick(251);QCOMPARE(m.state().y,100);m.tick(252);QCOMPARE(m.state().y,0);QVERIFY(!m.state().attendant);
        m.apply(in,253);QCOMPARE(m.state().y,0);QVERIFY(!m.state().attendant);
        in.hold=false;in.y=0;m.apply(in,254);QVERIFY(!m.state().attendant);
        in.request=2;m.apply(in,255);QVERIFY(m.state().attendant);QVERIFY(!m.state().held);
        in.hold=true;m.apply(in,256);QVERIFY(m.state().held);
        m.endSession();m.apply(in,257);QVERIFY(!m.state().linked);QVERIFY(!m.state().held);
        m.beginSession(300);QVERIFY(!m.state().attendant);QCOMPARE(m.state().limit,20);
    }
    void limitsPowerAndInvalidInput(){
        AttendantState m;m.beginSession(0);Input in;in.request=1;in.attendant=true;m.apply(in,1);in.hold=true;m.apply(in,2);in.x=100;in.y=100;m.apply(in,3);QVERIFY(std::hypot(m.state().x,m.state().y)<=101);
        in.limit=40;m.apply(in,4);QVERIFY(!m.state().held);QCOMPARE(m.state().x,0);
        in.x=in.y=0;m.apply(in,5);QVERIFY(!m.state().held);in.hold=false;m.apply(in,6);in.hold=true;m.apply(in,7);in.x=100;m.apply(in,8);QCOMPARE(m.state().x,100);
        in.x=101;QVERIFY(!m.apply(in,9));QCOMPARE(m.state().x,0);QVERIFY(!m.state().attendant);
        in.x=0;in.hold=false;in.request=2;m.apply(in,10);QVERIFY(m.state().attendant);
        in.power=false;m.apply(in,11);QVERIFY(!m.state().power);QVERIFY(!m.state().attendant);
        in.power=true;m.apply(in,12);QVERIFY(!m.state().attendant);
        in.request=3;m.apply(in,13);QVERIFY(m.state().attendant);
        in.request=2;QVERIFY(!m.apply(in,14));QVERIFY(!m.state().attendant);
    }
    void focusChangePreservesControlAndSettings(){
        AttendantController c;c.registerOnDataFrame();c.toggleDrivingControl();
        c.nextProfile();c.nextSpeed();
        const auto mode=c.modeNumber(), speed=c.inputLimit();
        const auto serial=c.benchSerial();
        c.setAxes(100,0);QCOMPARE(c.outputX(),100);
        c.setApplicationActive(false);
        QCOMPARE(c.outputX(),0);QVERIFY(c.attendantControl());QVERIFY(c.held());
        QCOMPARE(c.modeNumber(),mode);QCOMPARE(c.inputLimit(),speed);QCOMPARE(c.benchSerial(),serial);
        // Fresh neutral UI heartbeats preserve ownership in the background.
        for(int i=0;i<8;++i){c.setAxes(0,0);QTest::qWait(50);}
        QVERIFY(c.attendantControl());QVERIFY(c.held());QCOMPARE(c.outputX(),0);
        c.setAxes(100,100);QCOMPARE(c.inputX(),0);QCOMPARE(c.inputY(),0);
        c.setApplicationActive(true);QCOMPARE(c.outputX(),0);QVERIFY(c.attendantControl());
        QCOMPARE(c.modeNumber(),mode);QCOMPARE(c.inputLimit(),speed);
        c.setAxes(0,100);QCOMPARE(c.outputY(),100);
        c.toggleDrivingControl();QVERIFY(!c.attendantControl());
    }
    void backgroundStillRequiresFreshUiInput(){
        AttendantController c;c.registerOnDataFrame();c.toggleDrivingControl();c.setAxes(100,0);
        c.setApplicationActive(false);QCOMPARE(c.outputX(),0);QVERIFY(c.attendantControl());
        QTest::qWait(300);QVERIFY(!c.attendantControl());QCOMPARE(c.outputX(),0);
        c.setApplicationActive(true);QVERIFY(!c.attendantControl());
        c.setAxes(0,0);QVERIFY(!c.attendantControl());
    }
    void invalidInputStopsControl(){
        AttendantController c;c.registerOnDataFrame();c.requestControl();QVERIFY(c.attendantControl());c.setHold(true);c.setAxes(100,0);QCOMPARE(c.outputX(),100);
        c.setAxes(0,100);QCOMPARE(c.outputY(),100);
        c.setAxes(std::numeric_limits<double>::quiet_NaN(),0);QCOMPARE(c.outputY(),0);QVERIFY(!c.attendantControl());
        c.shutdown();QVERIFY(!c.canConnected());QCOMPARE(c.outputX(),0);
    }
};
QTEST_GUILESS_MAIN(AttendantTest)
#include "attendant_state_test.moc"
