#pragma once
#include <cstdint>

namespace attendant {
// Intent and state for the bench emulator, not an implemented R-net DAT driver.
struct Input {
    // limit stores drive-speed percentage; it never scales joystick travel.
    int x = 0, y = 0, limit = 20;
    bool power = true, attendant = false, hold = false;
    std::uint32_t request = 0;
};
enum class Reason {
    Ready, Passenger, Attendant, Moving, CenterFirst, ReleaseFirst,
    PowerOff, Stopped, LinkLost, Timeout, FocusLost, LimitChanged
};
struct State {
    int x = 0, y = 0, limit = 20;
    bool linked = false, power = false, attendant = false, held = false;
    Reason reason = Reason::LinkLost;
};
class AttendantState {
public:
    static constexpr int timeoutMs = 250;
    void beginSession(std::int64_t now);
    void endSession(Reason reason = Reason::LinkLost);
    bool apply(const Input &input, std::int64_t now);
    void tick(std::int64_t now);
    void stop(Reason reason = Reason::Stopped);
    const State &state() const { return state_; }
private:
    State state_;
    std::uint32_t lastRequest_ = 0;
    std::int64_t lastInput_ = 0;
    bool inputFresh_ = false, releaseRequired_ = true;
};
}
