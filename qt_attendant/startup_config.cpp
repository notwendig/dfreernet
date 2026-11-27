#include "startup_config.hpp"
#include <array>

namespace attendant {
namespace {
struct Exchange {
    const char *request;
    const char *reply;
    bool variable;
};
constexpr std::array<Exchange, 7> exchanges{{
    {"2380000010000000", "4080000000000000", false},
    {"4380000000000000", "2080000010000000", false},
    {"2381000006000100", "4081000000000000", false},
    {"438f000000000000", "208f000000000000", true},
    {"2381000002000000", "4081000000000000", false},
    {"438f000000000000", "208f000000000000", true},
    {"2380000000000000", "4080000000000000", false}
}};
QString requestAt(int index)
{
    return "780#" + QString::fromLatin1(exchanges[index].request);
}
}
QString StartupConfig::begin(qint64 now)
{
    reset();
    state_ = State::Loading;
    deadline_ = now + 1000;
    return requestAt(0);
}
QString StartupConfig::receive(quint32 id, const QByteArray &data, qint64 now)
{
    tick(now);
    if (state_ != State::Loading || id != 0x793 || data.size() != 8)
        return {};
    const auto expected = QByteArray::fromHex(exchanges[step_].reply);
    if ((static_cast<unsigned char>(data[0]) & 0xf0) == 0xc0 &&
        (static_cast<unsigned char>(data[0]) & 15) == 0 && data.mid(1,3) == expected.mid(1,3)) {
        state_ = State::Failed;
        error_ = QStringLiteral("Configuration request rejected");
        return {};
    }
    const bool matches = exchanges[step_].variable ?
        data.left(4) == expected.left(4) : data == expected;
    if (!matches) return {};
    values_[step_] = data.mid(4);
    ++step_;
    if (step_ == int(exchanges.size())) {
        state_ = State::Ready;
        return {};
    }
    deadline_ = now + 1000;
    return requestAt(step_);
}
void StartupConfig::tick(qint64 now)
{
    if (state_ == State::Loading && now >= deadline_) {
        state_ = State::Failed;
        error_ = QStringLiteral("Configuration response timed out");
    }
}
void StartupConfig::reset()
{
    step_ = 0;
    deadline_ = 0;
    state_ = State::Waiting;
    error_.clear();
    for (auto &value : values_) value.clear();
}
QByteArray StartupConfig::value(int index) const
{
    return index >= 0 && index < int(exchanges.size()) ? values_[index] : QByteArray();
}
}
