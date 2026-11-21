#include "prx/libSceVideoOut/include/Port0Presence.hpp"
#include "prx/libSceVideoOut/include/WindowTitleUpdate.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    Require(WindowTitleUpdate::ShouldPush(1, false), "first flip must push the initial title");
    Require(WindowTitleUpdate::ShouldPush(2, true), "a closed fps window must push the title");
    Require(!WindowTitleUpdate::ShouldPush(2, false), "a flip without a closed fps window must not push the title");
    Require(!WindowTitleUpdate::ShouldPush(60, false), "steady-state flips must not push the title");
    Require(WindowTitleUpdate::ShouldPush(121, true), "an fps window closed on a later flip must push the title");

    using Clock = Port0Presence::Presence::Clock;

    {
        Port0Presence::Presence presence(Port0Presence::Mode::Controller, false);
        Require(!presence.Observe(true, Clock::time_point::min()).has_value(),
                "starting hold at min time_point must stay pending");
        const auto commit = presence.Observe(true, Clock::time_point::max());
        Require(commit.has_value() && *commit && presence.Connected(),
                "hold from min to max time_point must commit without signed overflow");
    }

    {
        Port0Presence::Presence presence(Port0Presence::Mode::Controller, true);
        Require(!presence.Observe(false, Clock::time_point::max()).has_value(),
                "starting hold at max time_point must stay pending");
        const auto backward = presence.Observe(false, Clock::time_point::min() + Port0Presence::kHold);
        Require(!backward.has_value() && presence.Connected(),
                "backward observation must not commit or alter prior connected state");
    }

    {
        Port0Presence::Presence presence(Port0Presence::Mode::Controller, false);
        Require(!presence.Observe(true, Clock::time_point::max() - std::chrono::milliseconds(100)).has_value(),
                "starting hold near max time_point must stay pending");
        Require(!presence.Observe(true, Clock::time_point::max()).has_value() && !presence.Connected(),
                "hold shorter than kHold ending at max time_point must not overflow or commit");
    }

    {
        Port0Presence::Presence presence(Port0Presence::Mode::Controller, false);
        Require(!presence.Observe(true, Clock::time_point::max() - Port0Presence::kHold).has_value(),
                "starting hold at max minus kHold must stay pending");
        Require(!presence.Observe(true, Clock::time_point::max() - std::chrono::milliseconds(1)).has_value() &&
                    !presence.Connected(),
                "hold must remain pending 1ms before deadline at max time_point");
        const auto commit = presence.Observe(true, Clock::time_point::max());
        Require(commit.has_value() && *commit && presence.Connected(),
                "hold starting at max minus kHold must commit at max time_point");
    }

    return 0;
}
