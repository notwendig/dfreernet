#include "module_enrollment.hpp"
#include <QJsonArray>
#include <QRegularExpression>
#include <functional>

namespace attendant {
namespace {
QString emptyFrame(quint32 id)
{
    return QString("%1#").arg(id,8,16,QLatin1Char('0'));
}
}
bool ModuleEnrollment::configure(const QJsonObject &profile)
{
    initialRequested_ = false;
    reset();
    serial_.clear();
    tables_ = StartupTables{};
    expectedIdentities_.clear();
    keys_.clear();
    expectedWords_.clear();
    const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{16}$"));
    if (!hex.match(profile.value("identity").toString()).hasMatch() ||
        !hex.match(profile.value("announcement_keys").toString()).hasMatch()) return false;
    const auto serial = QByteArray::fromHex(profile.value("identity").toString().toLatin1());
    const auto keys = QByteArray::fromHex(profile.value("announcement_keys").toString().toLatin1());
    if (serial.size() != 8 || keys.size() != 8) return false;
    const auto words = profile.value("other_high_words").toArray();
    if (words.size() != 4) return false;
    const quint16 ownWord = quint8(serial[0]) | (quint16(quint8(serial[1])) << 8);
    for (const auto &word : words) {
        const auto number = word.toInt(-1);
        if (!word.isDouble() || word.toDouble()!=number || number < 0 || number > 65535 || number == ownWord) return false;
        expectedWords_.insert(quint16(number));
    }
    if (expectedWords_.size() != 4) return false;
    const auto records = profile.value("startup_tables").toArray();
    if (!records.isEmpty()) {
        if (!tables_.configure(records)) return false;
        const auto identities = profile.value("identities").toArray();
        if (identities.size() != 5) return false;
        for (int i=0;i<5;++i) {
            if (!hex.match(identities[i].toString()).hasMatch()) return false;
            const auto identity = QByteArray::fromHex(identities[i].toString().toLatin1());
            if (identity.size() != 8 || (i == 0 && identity != serial)) return false;
            expectedIdentities_.insert(unsigned(i),identity);
        }
    }
    if (!expectedIdentities_.isEmpty()) {
        QVector<QSet<quint16>> candidates;
        QSet<QByteArray> unique;
        for (unsigned slot=0;slot<5;++slot) {
            const auto identity = expectedIdentities_.value(slot);
            unique.insert(identity);
            if (slot) candidates.append(QSet<quint16>{
                quint16(quint8(identity[0]) | (quint16(quint8(identity[1])) << 8)),
                quint16((quint16(quint8(identity[0])) << 8) | quint8(identity[1]))});
        }
        // The captured vendor node uses big-endian arbitration words while
        // the other nodes use little-endian words. Require a one-to-one match
        // to the separately captured arbitration set, not one byte order.
        std::function<bool(int,QSet<quint16>)> matches = [&](int slot,QSet<quint16> remaining) {
            if (slot == candidates.size()) return remaining.isEmpty();
            for (auto word : candidates[slot]) {
                if (!remaining.contains(word)) continue;
                auto next=remaining;next.remove(word);
                if (matches(slot+1,next)) return true;
            }
            return false;
        };
        if (unique.size() != 5 || !matches(0,expectedWords_)) return false;
        for (const auto &value : records) {
            const auto record = value.toObject();
            const auto id = record.value("id").toString().toUInt(nullptr,16);
            const auto data = QByteArray::fromHex(record.value("data").toString().toLatin1());
            if ((id & 0xffffu) == 0xf060u && data != expectedIdentities_.value((id>>16)&15)) return false;
            if (id == 0x1ec0f062u && data != keys) return false;
        }
    }
    serial_ = serial;
    keys_ = keys;
    return true;
}
void ModuleEnrollment::boot(qint64 now)
{
    reset();
    if (!configured()) return;
    initialBoot_ = initialRequested_;
    initialRequested_ = false;
    running_ = advertising_ = true;
    nextAdvertisement_ = now + 200;
    deadline_ = now + 6000;
}
void ModuleEnrollment::reset()
{
    running_ = advertising_ = enumerated_ = metadataSent_ = configurationRequested_ = false;
    responses_ = advertisements_ = 0;
    serialParts_ = 0;
    normalConfirmed_ = discoveryEnded_ = false;
    normalPeers_.clear();
    normalReadyAt_ = 0;
    receivedWords_.clear();
    receivedIdentities_.clear();
    tablesStarted_ = false;
    tables_.reset();
    error_.clear();
}
void ModuleEnrollment::fail(const QString &reason)
{
    running_ = advertising_ = false;
    error_ = reason;
}
QStringList ModuleEnrollment::tick(qint64 now)
{
    if (!running_) return {};
    if (normalConfirmed_) {
        if (now >= normalReadyAt_) {
            configurationRequested_ = true;
            running_ = false;
        }
        return {};
    }
    if (tablesStarted_) {
        const auto frames = tables_.tick(now);
        if (!tables_.error().isEmpty()) fail(tables_.error());
        else if (tables_.complete()) running_ = false;
        return frames;
    }
    if (now >= deadline_) {
        running_ = false;
        error_ = QStringLiteral("Module enumeration timed out");
        return {};
    }
    if (!advertising_ || now < nextAdvertisement_) return {};
    if (!initialBoot_) {
        advertising_ = false;
        // Discard discovery from before this module joined the bus.
        normalPeers_.clear();
        discoveryEnded_ = false;
        QStringList frames;
        for (unsigned i=0;i<8;++i)
            frames.append(emptyFrame(0x1f000000u | (i<<20) |
                          (quint32(quint8(keys_[i]))<<8) | quint8(serial_[i])));
        return frames;
    }
    if (++advertisements_ > 20) {
        fail(QStringLiteral("No response to module announcement"));
        return {};
    }
    nextAdvertisement_ = now + 50;
    // insys.txt shows only this candidate announcement during arbitration.
    // Complete eight-part identity bursts belong to a different discovery phase.
    return {emptyFrame(0x1f000000u | (quint32(quint8(keys_[0])) << 8) | quint8(serial_[0]))};
}
QStringList ModuleEnrollment::receive(quint32 id, const QByteArray &data, qint64 now)
{
    if (!running_ || (!tablesStarted_ && now >= deadline_)) return {};
    if (!initialBoot_ && !advertising_ && data.isEmpty() &&
        id >= 0x1f800010 && id <= 0x1f800030) {
        if (!automatic_ || !liveProfileReady() || normalConfirmed_ || id != 0x1f800010) {
            fail(QStringLiteral("Unexpected module enumeration request"));
            return {};
        }
        // The network explicitly requests enumeration. Continue in this same
        // boot; never restart advertising or repeat a table transfer on timeout.
        initialBoot_ = true;
        normalPeers_.clear();
        discoveryEnded_ = false;
    }
    if (!initialBoot_) {
        // Early traffic still describes a network without this module.
        if (advertising_) return {};
        if (id == 0x7b0 && data.isEmpty()) discoveryEnded_ = true;
        if (data.isEmpty() && (id & 0xff000000u) == 0x1f000000u) {
            const unsigned part = (id >> 20) & 15, slot = (id >> 16) & 15;
            if (part < 8 && slot == 0 && quint8(id >> 8) == quint8(keys_[part])) {
                fail(QStringLiteral("Another device is announcing slot 0"));
                return {};
            }
            if (part < 8 && slot >= 1 && slot <= 4) {
                if (part == 0 && quint8(id >> 8) != quint8(keys_[0])) {
                    normalPeers_.remove(slot);
                    return {};
                }
                if (part > 0 && !normalPeers_.contains(slot)) return {};
                if (quint8(id >> 8) != quint8(keys_[part])) {
                    fail(QStringLiteral("Registered startup differs from the captured system"));
                    return {};
                }
                auto &identity = normalPeers_[slot];
                if (part == 0) identity.clear();
                if (unsigned(identity.size()) != part) {
                    fail(QStringLiteral("Registered startup differs from the captured system"));
                    return {};
                }
                identity.append(char(id & 255));
                if (identity.size() == 8 && identity != expectedIdentities_.value(slot)) {
                    fail(QStringLiteral("Registered startup differs from the captured system"));
                    return {};
                }
            }
        }
        bool peersComplete = normalPeers_.size() == 4;
        for (const auto &identity : normalPeers_) peersComplete &= identity.size() == 8;
        if (discoveryEnded_ && peersComplete && !normalConfirmed_) {
            normalConfirmed_ = true;
            advertising_ = false;
            // Original registered startup queries POP one bus tick after 7B0.
            normalReadyAt_ = now + 21;
        }
        return {};
    }
    if (enumerated_ && tables_.configured() && id >= 0x1fb00001 && id <= 0x1fb00004 && data.size() == 8) {
        const auto slot = id - 0x1fb00000;
        if (data != expectedIdentities_.value(slot)) {
            fail(QStringLiteral("Module enumeration differs from the captured system"));
            return {};
        }
        receivedIdentities_.insert(slot,data);
        if (!tablesStarted_ && receivedIdentities_.size() == 4) {
            tables_.begin(now);
            tablesStarted_ = true;
        }
    }
    if (tablesStarted_) tables_.receive(id,data);
    if (id == 0x7b0 && data.isEmpty()) advertising_ = false;
    if ((id & 0xffff0000u) == 0x1f930000u && data.isEmpty())
        receivedWords_.insert(quint16(id));
    if (data.isEmpty() && id >= 0x1f800010 && id <= 0x1f800013) {
        if (++responses_ > 32) {
            running_ = false;
            error_ = QStringLiteral("Repeated module enumeration requests");
            return {};
        }
        const unsigned index = id - 0x1f800010;
        serialParts_ |= 1u << index;
        const unsigned offset = (3 - index) * 2;
        const quint16 word = quint8(serial_[offset]) | (quint16(quint8(serial_[offset+1])) << 8);
        return {emptyFrame(0x1f900000u | (index << 16) | word)};
    }
    if (id == 0x1f800020 && data.isEmpty()) {
        if (serialParts_ != 15) {
            fail(QStringLiteral("Incomplete module enumeration"));
            return {};
        }
        const quint16 ownWord = quint8(serial_[0]) | (quint16(quint8(serial_[1])) << 8);
        auto peers = receivedWords_;
        peers.remove(ownWord);
        if (peers != expectedWords_) {
            running_ = false;
            error_ = QStringLiteral("Module enumeration differs from the captured system");
            return {};
        }
        unsigned rank = 0;
        for (const auto word : peers) if (word < ownWord) ++rank;
        // The captured attendant device announces class 1 and its serial rank.
        metadataSent_ = true;
        return {emptyFrame(0x1fa00010u | rank)};
    }
    if (id == 0x1f800030 && data.isEmpty()) {
        if (!metadataSent_) return {};
        enumerated_ = true;
        return {QString("1fb00000#") + serial_.toHex()};
    }
    // Last mode-table record preceding the slot-0 configuration queries.
    if (enumerated_ && id == 0x1ec42040 &&
        data == QByteArray::fromHex("0103000000000000")) {
        configurationRequested_ = true;
        if (!tables_.configured()) running_ = false;
    }
    return {};
}
}
