#include "startup_tables.hpp"
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>

namespace attendant {
bool StartupTables::configure(const QJsonArray &records)
{
    stages_.clear();
    reset();
    if (records.isEmpty() || records.size() > 512) return false;
    QHash<quint32,int> groups;
    QHash<quint32,qint64> origins;
    const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{16}$"));
    qint64 previous = -1;
    for (const auto &entry : records) {
        const auto object = entry.toObject();
        bool ok = false;
        const auto id = object.value("id").toString().toUInt(&ok,16);
        const auto payload = object.value("data").toString();
        const auto time = object.value("ms").toInt(-1);
        const auto slot = (id >> 16) & 15;
        if (!ok || (id & 0xfff00000u) != 0x1ec00000u || slot > 4 ||
            !hex.match(payload).hasMatch() || time < previous || time > 20000) {
            stages_.clear();
            return false;
        }
        previous = time;
        const auto group = id & 0xffe0u;
        if (!groups.contains(group)) {
            groups.insert(group,stages_.size());
            origins.insert(group,time);
            stages_.append(Stage{});
        }
        auto &stage = stages_[groups[group]];
        Record record{id,QByteArray::fromHex(payload.toLatin1()),time-origins[group]};
        if (slot == 0) stage.own.append(record);
        else stage.peers.append(record);
        stage.duration = std::max(stage.duration,record.offset);
    }
    for (const auto &stage : stages_) {
        if (stage.own.isEmpty() || stage.peers.isEmpty()) {
            stages_.clear();
            return false;
        }
    }
    return true;
}
void StartupTables::reset()
{
    stage_ = -1;
    sent_ = 0;
    started_ = overallDeadline_ = 0;
    complete_ = false;
    seen_.clear();
    error_.clear();
}
void StartupTables::begin(qint64 now)
{
    reset();
    if (!configured()) return;
    for (const auto &stage : stages_) seen_.append(QVector<bool>(stage.peers.size(),false));
    stage_ = 0;
    // The reference starts its first table approximately 63 ms after identity.
    started_ = now + 60;
    overallDeadline_ = now + 30000;
}
void StartupTables::receive(quint32 id, const QByteArray &data)
{
    if (stage_ < 0 || complete_ || !error_.isEmpty()) return;
    // Receive ordering can differ across sockets; retain matching early replies.
    // Consume a duplicate at most once, including repeated segmented records.
    for (int s=stage_;s<stages_.size();++s) {
        for (int i=0;i<stages_[s].peers.size();++i) {
            const auto &record = stages_[s].peers[i];
            if (!seen_[s][i] && record.id == id && record.data == data) {
                seen_[s][i] = true;
                return;
            }
        }
    }
}
QStringList StartupTables::tick(qint64 now)
{
    if (stage_ < 0 || complete_ || !error_.isEmpty() || now < started_) return {};
    if (now >= overallDeadline_) {
        error_ = QStringLiteral("Startup table transfer timed out");
        return {};
    }
    const auto &stage = stages_[stage_];
    if (now - started_ > stage.duration + 1500) {
        error_ = QStringLiteral("Startup table peer response timed out");
        return {};
    }
    QStringList frames;
    // One frame per timer pulse avoids replay bursts after a delayed UI tick.
    bool ready = sent_ < stage.own.size() && now-started_ >= stage.own[sent_].offset;
    if (sent_ < stage.own.size() && stage.own[sent_].id == 0x1ec03061) {
        // PM publishes each profile record on 33060. 3061 is the BGM reply,
        // not a timed broadcast: even a delayed PM must precede our response.
        ready = false;
        for (int i=0;i<stage.peers.size();++i) {
            const auto &peer = stage.peers[i];
            if (peer.id == 0x1ec33060 && seen_[stage_][i] &&
                peer.data.mid(1) == stage.own[sent_].data.mid(1)) {
                ready = true;
                break;
            }
        }
    }
    if (ready) {
        const auto &record = stage.own[sent_++];
        frames.append(QString("%1#%2").arg(record.id,8,16,QLatin1Char('0'))
                      .arg(QString::fromLatin1(record.data.toHex())));
    }
    if (sent_ == stage.own.size() &&
        std::all_of(seen_[stage_].cbegin(),seen_[stage_].cend(),[](bool seen){return seen;})) {
        if (++stage_ == stages_.size()) complete_ = true;
        else { sent_ = 0; started_ = now + 10; }
    }
    return frames;
}
}
