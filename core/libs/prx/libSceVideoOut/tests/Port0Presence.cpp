#include "prx/libSceVideoOut/include/Port0Presence.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "video_out_port0_presence: %s\n", message);
    std::exit(1);
}

using Port0Presence::Mode;
using Port0Presence::Presence;
using Clock = Presence::Clock;

Clock::time_point At(std::chrono::milliseconds offset) {
    return Clock::time_point{} + offset;
}

void ParsesOnlyTheTwoNames() {
    Require(Port0Presence::ParseMode(nullptr) == Mode::Default, "an unset variable keeps the default");
    Require(Port0Presence::ParseMode("default") == Mode::Default, "default is accepted");
    Require(Port0Presence::ParseMode("controller") == Mode::Controller, "controller is accepted");
    for (const char* bad : {"", "Controller", "on", "1", " controller", "controller ", "pad"}) {
        bool threw = false;
        std::string message;
        try {
            Port0Presence::ParseMode(bad);
        } catch (const std::runtime_error& error) {
            threw = true;
            message = error.what();
        }
        Require(threw, "an invalid value must throw");
        Require(message.find("ANYPS5_PAD_PORT0_STATE") != std::string::npos, "the error must name the variable");
    }
}

void DefaultStaysConnected() {
    Presence unbound(Mode::Default, false);
    Require(unbound.Connected(), "default mode starts connected without a controller");
    Require(!unbound.Observe(false, At(std::chrono::milliseconds{0})).has_value(), "default mode reports no change");
    Require(!unbound.Observe(false, At(std::chrono::seconds{10})).has_value(), "default mode never reports a change");
    Require(unbound.Connected(), "default mode stays connected");
    Presence bound(Mode::Default, true);
    Require(bound.Connected() && !bound.Observe(true, At(std::chrono::milliseconds{0})).has_value(), "default mode with a controller");
}

void ControllerStartsFromTheController() {
    Presence unbound(Mode::Controller, false);
    Require(!unbound.Connected(), "controller mode without a controller starts disconnected");
    Require(!unbound.Observe(false, At(std::chrono::seconds{5})).has_value(), "no controller, no change");
    Presence bound(Mode::Controller, true);
    Require(bound.Connected(), "controller mode with a controller starts connected");
}

void RemovalHoldsForTheHoldTime() {
    Presence presence(Mode::Controller, true);
    Require(!presence.Observe(false, At(std::chrono::seconds{1})).has_value(), "a removal starts the hold");
    Require(presence.Connected(), "still connected during the hold");
    Require(!presence.Observe(false, At(std::chrono::seconds{1}) + Port0Presence::kHold / 2).has_value(), "half way through the hold");
    Require(presence.Connected(), "still connected half way through the hold");
    Require(!presence.Observe(false, At(std::chrono::seconds{1}) + Port0Presence::kHold - std::chrono::milliseconds{1}).has_value(), "one millisecond short of the hold");
    Require(presence.Connected(), "still connected one millisecond short of the hold");
    const auto change = presence.Observe(false, At(std::chrono::seconds{1}) + Port0Presence::kHold);
    Require(change.has_value() && !*change, "the removal is reported once the hold has passed");
    Require(!presence.Connected(), "disconnected after the hold");
    Require(!presence.Observe(false, At(std::chrono::seconds{9})).has_value(), "a second report of the same state is silent");
}

void FlapInsideTheHoldIsIgnored() {
    Presence presence(Mode::Controller, true);
    Require(!presence.Observe(false, At(std::chrono::seconds{1})).has_value(), "first removal sample");
    Require(!presence.Observe(true, At(std::chrono::seconds{1}) + std::chrono::milliseconds{100}).has_value(), "reattached inside the hold");
    Require(presence.Connected(), "a reattach inside the hold keeps the pad connected");
    Require(!presence.Observe(true, At(std::chrono::seconds{30})).has_value(), "no late report after a reattach");
    Require(presence.Connected(), "still connected after the late sample");
    Require(!presence.Observe(false, At(std::chrono::seconds{31})).has_value(), "the next removal starts a new hold");
    Require(presence.Connected(), "the new hold has not passed");
}

void ReconnectHoldsForTheHoldTime() {
    Presence presence(Mode::Controller, false);
    Require(!presence.Observe(true, At(std::chrono::seconds{2})).has_value(), "an attach starts the hold");
    Require(!presence.Connected(), "still disconnected during the hold");
    Require(!presence.Observe(true, At(std::chrono::seconds{2}) + Port0Presence::kHold / 2).has_value(), "half way through the reconnect hold");
    Require(!presence.Connected(), "still disconnected half way through the reconnect hold");
    Require(!presence.Observe(true, At(std::chrono::seconds{2}) + Port0Presence::kHold - std::chrono::milliseconds{1}).has_value(), "one millisecond short of the reconnect hold");
    const auto change = presence.Observe(true, At(std::chrono::seconds{2}) + Port0Presence::kHold);
    Require(change.has_value() && *change, "the attach is reported once the hold has passed");
    Require(presence.Connected(), "connected after the hold");
}

}

int main() {
    ParsesOnlyTheTwoNames();
    DefaultStaysConnected();
    ControllerStartsFromTheController();
    RemovalHoldsForTheHoldTime();
    FlapInsideTheHoldIsIgnored();
    ReconnectHoldsForTheHoldTime();
    std::puts("video_out_port0_presence: all checks passed");
    return 0;
}
