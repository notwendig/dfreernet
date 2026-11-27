#include "attendant_state.hpp"
#include <algorithm>
#include <cmath>
namespace attendant {
void AttendantState::beginSession(std::int64_t now) {
    state_ = {}; state_.linked = true; state_.power = true; state_.reason = Reason::Ready;
    lastRequest_ = 0; lastInput_ = now; inputFresh_ = false; releaseRequired_ = true;
}
void AttendantState::stop(Reason reason) {
    state_.x = state_.y = 0; state_.held = false; state_.attendant = false;
    state_.reason = reason; releaseRequired_ = true;
}
void AttendantState::endSession(Reason reason) {
    stop(reason); state_.linked = state_.power = false; inputFresh_ = false;
}
bool AttendantState::apply(const Input &in, std::int64_t now) {
    if (!state_.linked) return false;
    if (now < lastInput_ || in.request < lastRequest_ || in.x < -100 || in.x > 100 || in.y < -100 || in.y > 100 || in.limit < 0 || in.limit > 100) { stop(); return false; }
    // Expire before accepting a delayed packet. An old takeover request cannot
    // restore authority after the watchdog has removed it.
    tick(now);
    const bool neutral = std::abs(in.x) <= 5 && std::abs(in.y) <= 5;
    if (in.limit != state_.limit) {
        state_.held = false; releaseRequired_ = true; state_.reason = Reason::LimitChanged;
        state_.limit = in.limit;
    }
    if (!in.power) {
        stop(Reason::PowerOff); state_.power = false;
    } else if (!state_.power) {
        state_.power = true; state_.reason = Reason::Passenger;
    }
    if (in.request > lastRequest_) {
        lastRequest_ = in.request;
        if (!in.attendant) stop(Reason::Passenger);
        else if (state_.power && neutral && !in.hold) {
            state_.attendant = true; state_.held = false; releaseRequired_ = false; state_.reason = Reason::Attendant;
        } else {
            stop(in.hold ? Reason::ReleaseFirst : Reason::CenterFirst);
        }
    }
    if (!in.hold) {
        state_.held = false; releaseRequired_ = false;
    } else if (!state_.held) {
        if (state_.power && state_.attendant && !releaseRequired_ && neutral) state_.held = true;
        else { releaseRequired_ = true; if (state_.attendant) state_.reason = neutral ? Reason::ReleaseFirst : Reason::CenterFirst; }
    }
    state_.x = state_.y = 0;
    if (state_.power && state_.attendant && state_.held) {
        const double magnitude = std::hypot(double(in.x), double(in.y));
        // Joystick travel is always full range. Drive speed is sent separately
        // on CAN; applying it here would reduce both driving and actuator input.
        const double scale = magnitude > 100 ? 100.0/magnitude : 1.0;
        if (!neutral) { state_.x = std::lround(in.x*scale); state_.y = std::lround(in.y*scale); }
        state_.reason = state_.x || state_.y ? Reason::Moving : Reason::Attendant;
    }
    lastInput_ = now; inputFresh_ = true; return true;
}
void AttendantState::tick(std::int64_t now) {
    if (inputFresh_ && (now < lastInput_ || now-lastInput_ >= timeoutMs)) {
        stop(Reason::Timeout); inputFresh_ = false;
    }
}
}
