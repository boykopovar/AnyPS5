#include "prx/libScePad/include/PadIdentify.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "pad_identify: %s\n", message);
    std::exit(1);
}

void PatternHasOnePulsePerPlayer() {
    for (unsigned player = 1; player <= Pad::kIdentifyMaxPlayer; ++player) {
        const auto pattern = Pad::MakeIdentifyPattern(player);
        Require(pattern.count == player, "player N gets exactly N pulses");
        for (unsigned index = 0; index < player; ++index) {
            const auto& pulse = pattern.pulses[index];
            Require(pulse.startMs == index * 400u, "pulses start 400 ms apart from the bind point");
            Require(pulse.durationMs == 150u, "each pulse lasts 150 ms");
            Require(pulse.strength == 255, "each pulse is at full strength before the user scale");
        }
    }
}

void OutOfRangePlayersGetNoPattern() {
    for (const unsigned player : {0u, 5u, 6u, 0xFFFFFFFFu}) {
        const auto pattern = Pad::MakeIdentifyPattern(player);
        Require(pattern.count == 0, "a player outside 1 to 4 gets no pulses");
    }
}

bool RefusesMode(const char* text) {
    try {
        Pad::ParseIdentifyMode(text);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        return message.find("ANYPS5_PAD_IDENTIFY") != std::string::npos && message.find(text) != std::string::npos;
    }
    return false;
}

void ModeParserAcceptsOnlyTheThreeNames() {
    Require(Pad::ParseIdentifyMode(nullptr) == Pad::IdentifyMode::Off, "an unset variable keeps identify off");
    Require(Pad::ParseIdentifyMode("off") == Pad::IdentifyMode::Off, "off is accepted");
    Require(Pad::ParseIdentifyMode("connect") == Pad::IdentifyMode::Connect, "connect is accepted");
    Require(Pad::ParseIdentifyMode("always") == Pad::IdentifyMode::Always, "always is accepted");
    const char* refused[] = {"", "on", "1", "0", "Connect", "ALWAYS", " connect", "connect ", "always\n", "all", "off,connect"};
    for (const char* text : refused) Require(RefusesMode(text), "an invalid value is refused with an error naming the variable and the value");
}


using Clock = std::chrono::steady_clock;

struct Call {
    std::int64_t atMs;
    std::uint16_t low;
    std::uint16_t high;
    std::uint32_t durationMs;
};

std::vector<Call> Simulate(Pad::IdentifyScheduler& scheduler, Clock::time_point start, std::int64_t endMs, unsigned percent,
    const std::function<bool(std::int64_t)>& titleActive) {
    std::vector<Call> out;
    for (std::int64_t ms = 0; ms <= endMs; ms += 10) {
        const bool active = titleActive(ms);
        const std::size_t before = out.size();
        const bool sent = scheduler.Tick(start + std::chrono::milliseconds(ms), active, percent,
            [&out, ms](std::uint16_t low, std::uint16_t high, std::uint32_t durationMs) { out.push_back({ms, low, high, durationMs}); });
        Require(sent == (out.size() > before), "Tick reports a send exactly when the sink is called");
        if (active) Require(out.size() == before, "nothing is sent while the title is rumbling");
    }
    return out;
}

const auto Never = [](std::int64_t) { return false; };

void ConnectSendsEachPlayersPatternOnceAfterTheQuietWindow() {
    for (unsigned player = 1; player <= Pad::kIdentifyMaxPlayer; ++player) {
        Pad::IdentifyScheduler scheduler;
        const auto start = Clock::now();
        scheduler.Bind(start, player, Pad::IdentifyMode::Connect);
        const auto calls = Simulate(scheduler, start, 65000, 100, Never);
        Require(calls.size() == player, "connect sends N pulses for player N and never repeats them");
        for (unsigned index = 0; index < player; ++index) {
            Require(calls[index].atMs == 500 + 400 * static_cast<std::int64_t>(index), "pulses start after the 500 ms quiet window, 400 ms apart");
            Require(calls[index].low == 65535 && calls[index].high == 65535 && calls[index].durationMs == 150, "each pulse is a full-strength 150 ms command on both motors");
        }
    }
}

void TitleRumbleDelaysTheStartAndCancelsThePattern() {
    Pad::IdentifyScheduler delayed;
    const auto start = Clock::now();
    delayed.Bind(start, 1, Pad::IdentifyMode::Connect);
    const auto late = Simulate(delayed, start, 5000, 100, [](std::int64_t ms) { return ms < 300; });
    Require(late.size() == 1 && late[0].atMs == 790, "a title rumbling until 290 ms moves the quiet window to 790 ms");

    Pad::IdentifyScheduler cancelled;
    cancelled.Bind(start, 3, Pad::IdentifyMode::Connect);
    const auto cut = Simulate(cancelled, start, 10000, 100, [](std::int64_t ms) { return ms >= 1100 && ms < 1110; });
    Require(cut.size() == 2 && cut[0].atMs == 500 && cut[1].atMs == 900, "a title rumble before the third pulse stops the pattern at once");
}

void TitleRumbleIsNeverOverlapped() {
    Pad::IdentifyScheduler scheduler;
    const auto start = Clock::now();
    scheduler.Bind(start, 4, Pad::IdentifyMode::Always);
    const auto calls = Simulate(scheduler, start, 90000, 100, [](std::int64_t ms) { return ms % 1000 < 300; });
    for (const auto& call : calls) Require(call.atMs % 1000 >= 300, "no pulse is sent during a title rumble burst");
}

void AlwaysRepeatsThirtySecondsAfterTheLastPulse() {
    Pad::IdentifyScheduler scheduler;
    const auto start = Clock::now();
    scheduler.Bind(start, 1, Pad::IdentifyMode::Always);
    const auto calls = Simulate(scheduler, start, 65000, 100, Never);
    Require(calls.size() == 3 && calls[0].atMs == 500 && calls[1].atMs == 30500 && calls[2].atMs == 60500, "always repeats the pattern 30 seconds after each pulse");
}

void ZeroPercentSendsNothing() {
    Pad::IdentifyScheduler scheduler;
    const auto start = Clock::now();
    scheduler.Bind(start, 2, Pad::IdentifyMode::Always);
    Require(Simulate(scheduler, start, 65000, 0, Never).empty(), "0 percent never sends a pulse, not even a stop");
}

void UserScaleAppliesToThePulse() {
    Pad::IdentifyScheduler half;
    const auto start = Clock::now();
    half.Bind(start, 1, Pad::IdentifyMode::Connect);
    const auto halfCalls = Simulate(half, start, 1000, 50, Never);
    Require(halfCalls.size() == 1 && halfCalls[0].low == 32768 && halfCalls[0].high == 32768, "50 percent halves the full-strength pulse with .5 rounded up");

    Pad::IdentifyScheduler faint;
    faint.Bind(start, 1, Pad::IdentifyMode::Connect);
    const auto faintCalls = Simulate(faint, start, 1000, 1, Never);
    Require(faintCalls.size() == 1 && faintCalls[0].low == 655 && faintCalls[0].high == 655, "1 percent of the pulse is 655 on each motor");
}

void OffAndOutOfRangeNeverArm() {
    Pad::IdentifyScheduler off;
    const auto start = Clock::now();
    off.Bind(start, 1, Pad::IdentifyMode::Off);
    Require(Simulate(off, start, 65000, 100, Never).empty(), "off sends nothing");

    Pad::IdentifyScheduler outside;
    outside.Bind(start, 5, Pad::IdentifyMode::Always);
    Require(Simulate(outside, start, 65000, 100, Never).empty(), "player 5 has no pattern and sends nothing");
}

void RebindStartsANewPattern() {
    Pad::IdentifyScheduler scheduler;
    const auto start = Clock::now();
    scheduler.Bind(start, 1, Pad::IdentifyMode::Connect);
    Require(Simulate(scheduler, start, 5000, 100, Never).size() == 1, "the first binding sends its pattern");
    const auto again = start + std::chrono::seconds(70);
    scheduler.Bind(again, 1, Pad::IdentifyMode::Connect);
    const auto calls = Simulate(scheduler, again, 5000, 100, Never);
    Require(calls.size() == 1 && calls[0].atMs == 500, "a new binding to a slot starts its pattern again");
}

}

int main() {
    PatternHasOnePulsePerPlayer();
    OutOfRangePlayersGetNoPattern();
    ModeParserAcceptsOnlyTheThreeNames();
    ConnectSendsEachPlayersPatternOnceAfterTheQuietWindow();
    TitleRumbleDelaysTheStartAndCancelsThePattern();
    TitleRumbleIsNeverOverlapped();
    AlwaysRepeatsThirtySecondsAfterTheLastPulse();
    ZeroPercentSendsNothing();
    UserScaleAppliesToThePulse();
    OffAndOutOfRangeNeverArm();
    RebindStartsANewPattern();
    return 0;
}
