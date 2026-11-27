#include "startup_ready.hpp"

namespace attendant {
void StartupReady::reset()
{
    state_ = State::Waiting;
    initialized_ = readyPeers_ = 0;
    announced_ = false;
    deadline_ = 0;
    error_.clear();
}
QStringList StartupReady::begin(qint64 now)
{
    reset();
    state_ = State::ReadingFlags;
    deadline_ = now + 1000;
    return {QStringLiteral("780#4340000000000000")};
}
QStringList StartupReady::advance()
{
    if (state_ != State::WaitingPeers) return {};
    QStringList frames;
    if (initialized_ == 15 && !announced_) {
        announced_ = true;
        frames.append(QStringLiteral("040#80000000"));
    }
    if (announced_ && readyPeers_ == 15) state_ = State::Ready;
    return frames;
}
QStringList StartupReady::receive(quint32 id, const QByteArray &data, qint64 now)
{
    tick(now);
    if (!running()) return {};
    if (id >= 0x041 && id <= 0x044 && data.size() == 4) {
        const unsigned bit = 1u << (id-0x041);
        if (data == QByteArray::fromHex("00000000")) initialized_ |= bit;
        if (data == QByteArray::fromHex("80000000")) readyPeers_ |= bit;
        return advance();
    }
    if (id != 0x793 || data.size() != 8 || state_ == State::WaitingPeers) return {};
    const auto odi = state_ == State::ReadingFlags ? 0x40 : 0x50;
    if (quint8(data[1]) != odi || data[2] || data[3]) return {};
    if (quint8(data[0]) == 0xc0) {
        state_ = State::Failed;
        error_ = QStringLiteral("Configuration request rejected");
        return {};
    }
    if (quint8(data[0]) != 0x20) return {};
    if (state_ == State::ReadingFlags) {
        state_ = State::ReadingMode;
        deadline_ = now + 1000;
        return {QStringLiteral("780#4350000000000000")};
    }
    state_ = State::WaitingPeers;
    deadline_ = now + 3000;
    auto frames = QStringList{QStringLiteral("040#00000000")};
    frames.append(advance());
    return frames;
}
void StartupReady::tick(qint64 now)
{
    if (running() && now >= deadline_) {
        state_ = State::Failed;
        error_ = QStringLiteral("Module readiness confirmation timed out");
    }
}
}
