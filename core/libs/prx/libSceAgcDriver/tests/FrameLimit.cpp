#include "prx/libSceAgcDriver/Execution/include/FrameLimit.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::FramePacer;
using Nanos = FramePacer::Nanos;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

Nanos exactNanos(std::uint64_t frames, std::uint64_t milliFps) {
    return static_cast<Nanos>(static_cast<unsigned __int128>(frames) * 1'000'000'000'000ull / milliFps);
}

void deadlineArithmeticTests() {
    for (const std::uint64_t milliFps : {30000ull, 59940ull, 60000ull, 119880ull, 144000ull, 100000000ull, 500ull, 7ull * 1000ull + 333ull}) {
        FramePacer pacer(milliFps);
        Nanos now = 0;
        require(pacer.Admit(now) == 0, "the first frame of a run is delayed at " + std::to_string(milliFps));
        for (std::uint64_t frame = 1; frame <= 1'000'000; ++frame) {
            now += pacer.Admit(now);
            require(now == exactNanos(frame, milliFps), "frame " + std::to_string(frame) + " at limit " + std::to_string(milliFps) + " is presented at " + std::to_string(now) + " instead of " + std::to_string(exactNanos(frame, milliFps)));
        }
    }
}

void overshootDoesNotAccumulateTests() {
    constexpr std::uint64_t milliFps = 59940;
    FramePacer pacer(milliFps);
    Nanos now = 0;
    std::uint64_t seed = 12345;
    pacer.Admit(now);
    for (std::uint64_t frame = 1; frame <= 100'000; ++frame) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        const Nanos overshoot = static_cast<Nanos>((seed >> 33) % 2'000'000);
        now += pacer.Admit(now) + overshoot;
        const Nanos late = now - exactNanos(frame, milliFps);
        require(late >= 0 && late < 2'000'000, "a sleep that overshoots by under 2 ms leaves frame " + std::to_string(frame) + " " + std::to_string(late) + " ns after its deadline");
    }
}

void stallDoesNotBurstTests() {
    constexpr std::uint64_t milliFps = 30000;
    const Nanos period = exactNanos(1, milliFps);
    for (const Nanos stall : {Nanos{50'000'000}, Nanos{1'000'000'000}, Nanos{3'600'000'000'000}}) {
        FramePacer pacer(milliFps);
        Nanos now = 0;
        pacer.Admit(now);
        for (int frame = 1; frame <= 10; ++frame) now += pacer.Admit(now);
        now += stall;
        std::vector<Nanos> presents;
        for (int frame = 0; frame < 200; ++frame) {
            now += pacer.Admit(now);
            presents.push_back(now);
            now += 1'000'000;
        }
        for (std::size_t index = 0; index + 2 < presents.size(); ++index) {
            require(presents[index + 2] - presents[index] >= period - 1, "after a stall of " + std::to_string(stall) + " ns three presents fall within " + std::to_string(presents[index + 2] - presents[index]) + " ns at frame " + std::to_string(index));
        }
        require(presents[presents.size() - 1] - presents[2] >= static_cast<Nanos>(presents.size() - 3) * period - 2, "frames after a stall of " + std::to_string(stall) + " ns are not paced at the limit");
    }
}

void lateFrameIsMadeUpTests() {
    constexpr std::uint64_t milliFps = 30000;
    const Nanos period = exactNanos(1, milliFps);
    FramePacer pacer(milliFps);
    Nanos now = 0;
    pacer.Admit(now);
    for (std::uint64_t frame = 1; frame <= 4; ++frame) now += pacer.Admit(now);
    require(now == exactNanos(4, milliFps), "the frames before the late one are off the schedule");
    now += period + period / 2;
    require(pacer.Admit(now) == 0, "a late frame is delayed");
    const Nanos lateAt = now;
    const Nanos wait = pacer.Admit(now);
    require(wait == exactNanos(6, milliFps) - lateAt, "the frame after one late by half a period waits " + std::to_string(wait) + " ns instead of making up the delay");
    now += wait;
    require(now == exactNanos(6, milliFps), "the schedule did not resume after a late frame");
}

void slowGameIsNeverDelayedTests() {
    for (const std::uint64_t milliFps : {30000ull, 59940ull, 144000ull}) {
        FramePacer pacer(milliFps);
        const Nanos period = exactNanos(1, milliFps);
        Nanos now = 5'000'000'000;
        for (int frame = 0; frame < 10'000; ++frame) {
            require(pacer.Admit(now) == 0, "a game slower than the limit " + std::to_string(milliFps) + " is delayed at frame " + std::to_string(frame));
            now += period + period / 1000 + 1;
        }
    }
}

struct FakeTime {
    Nanos now = 0;
    Nanos overshoot = 0;
    std::vector<Nanos> requests;
    bool refuse = false;

    auto clock() { return [this] { return now; }; }
    auto sleeper() {
        return [this](Nanos length) {
            requests.push_back(length);
            now += length + overshoot;
            return !refuse;
        };
    }
};

void paceTests() {
    constexpr std::uint64_t milliFps = 30000;
    FramePacer pacer(milliFps);
    FakeTime time;
    time.now = 777;
    require(pacer.Pace(time.clock(), time.sleeper(), 20'000'000) && time.requests.empty(), "the first frame of a run sleeps");
    require(pacer.Pace(time.clock(), time.sleeper(), 20'000'000), "a completed wait reports an abandoned one");
    require(time.requests.size() == 2 && time.requests[0] == 20'000'000 && time.requests[1] == exactNanos(1, milliFps) - 20'000'000, "a wait of one period is not cut into a 20 ms slice and the rest");
    require(time.now == 777 + exactNanos(1, milliFps), "the wait ended at " + std::to_string(time.now - 777) + " ns instead of one period");
    time.requests.clear();
    time.overshoot = 3'000'000;
    pacer.Pace(time.clock(), time.sleeper(), 1'000'000'000);
    require(time.requests.size() == 1 && time.now >= 777 + exactNanos(2, milliFps), "an oversleep is waited for again");
    time.requests.clear();
    const Nanos beforeNext = time.now;
    pacer.Pace(time.clock(), time.sleeper(), 1'000'000'000);
    require(time.requests.size() == 1 && time.requests[0] == 777 + exactNanos(3, milliFps) - beforeNext, "the frame after an oversleep does not wait for its own deadline on the original schedule");
}

void paceOvershootTests() {
    FramePacer pacer(30000);
    FakeTime time;
    pacer.Pace(time.clock(), time.sleeper(), 5'000'000);
    time.overshoot = 3'000'000;
    pacer.Pace(time.clock(), time.sleeper(), 5'000'000);
    require(time.now >= exactNanos(1, 30000) && time.now < exactNanos(1, 30000) + 8'000'000, "sleeping in slices that overshoot by 3 ms ends " + std::to_string(time.now - exactNanos(1, 30000)) + " ns after the deadline");
    require(time.requests.size() <= 5, "a sleeper that overshoots is asked " + std::to_string(time.requests.size()) + " times for one wait");
}

void paceAbandonTests() {
    FramePacer pacer(30000);
    FakeTime time;
    pacer.Pace(time.clock(), time.sleeper(), 5'000'000);
    time.refuse = true;
    require(!pacer.Pace(time.clock(), time.sleeper(), 5'000'000), "a sleeper that refuses does not abandon the wait");
    require(time.requests.size() == 1, "the wait goes on after a sleeper refused");
}

void paceSlowGameTests() {
    FramePacer pacer(60000);
    FakeTime time;
    for (int frame = 0; frame < 1000; ++frame) {
        pacer.Pace(time.clock(), time.sleeper(), 5'000'000);
        time.now += 20'000'000;
    }
    require(time.requests.empty(), "a game at 50 frames per second slept " + std::to_string(time.requests.size()) + " times under a limit of 60");
}

void paceHighLimitTests() {
    FramePacer pacer(100'000'000);
    FakeTime time;
    pacer.Pace(time.clock(), time.sleeper(), 1'000'000);
    for (int frame = 0; frame < 1000; ++frame) {
        pacer.Pace(time.clock(), time.sleeper(), 1'000'000);
        require(time.now == exactNanos(static_cast<std::uint64_t>(frame) + 1, 100'000'000), "a limit of 100000 frames per second misses frame " + std::to_string(frame));
    }
}

void resetTests() {
    FramePacer pacer(30000);
    pacer.Admit(0);
    require(pacer.Admit(0) == exactNanos(1, 30000), "the second frame at once is not delayed by a period");
    pacer.Reset();
    require(pacer.Admit(0) == 0, "a frame after a reset is delayed");
    require(pacer.Admit(0) == exactNanos(1, 30000), "the schedule does not restart after a reset");
}

std::string refusal(const char* text) {
    try {
        AgcDriver::ParseFrameLimit(text);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

std::uint64_t parsedMilli(const char* text) {
    const auto limit = AgcDriver::ParseFrameLimit(text);
    require(limit.has_value(), std::string("'") + text + "' is read as no limit");
    return limit->milliFramesPerSecond;
}

void parseAcceptedTests() {
    require(parsedMilli("30") == 30000 && parsedMilli("60") == 60000 && parsedMilli("144") == 144000, "a whole number of frames per second is misread");
    require(parsedMilli("59.94") == 59940 && parsedMilli("119.88") == 119880 && parsedMilli("29.97") == 29970, "a decimal limit is misread");
    require(parsedMilli("0.5") == 500 && parsedMilli("0.500") == 500 && parsedMilli("1.25") == 1250 && parsedMilli("7.001") == 7001, "a limit below 10 is misread");
    require(parsedMilli("100000") == 100'000'000 && parsedMilli("1000000") == 1'000'000'000 && parsedMilli("1000000.000") == 1'000'000'000, "a very high limit is misread");
    require(parsedMilli("030") == 30000 && parsedMilli("30.000") == 30000, "leading or trailing zeros change the limit");
    require(std::abs(AgcDriver::ParseFrameLimit("59.94")->FramesPerSecond() - 59.94) < 1e-9, "the limit in frames per second is not the parsed value");
}

void parseDisabledTests() {
    for (const char* text : {"0", "00", "0.0", "0.000", "000.00"}) require(!AgcDriver::ParseFrameLimit(text).has_value(), std::string("'") + text + "' is a limit");
    require(!AgcDriver::ParseFrameLimit(nullptr).has_value() && !AgcDriver::ParseFrameLimit("").has_value(), "an unset or empty ANYPS5_FPS_LIMIT is a limit");
}

void parseRefusedTests() {
    for (const char* text : {"-30", "+30", "-0", "abc", "30fps", "30 fps", " 30", "30 ", "1e2", "0x20", "30.", ".5", ".", "30.1234", "30.0001", "0.4", "0.499", "0.001", "1000000.001", "1000001", "99999999999999999999", "18446744073709551616", "nan", "inf", "30,5", "3 0", "30\n", "\xd9\xa3\xd9\xa0", "0.5.0", "--5"}) {
        const auto message = refusal(text);
        require(!message.empty(), std::string("'") + text + "' was accepted");
        require(message.find("ANYPS5_FPS_LIMIT") != std::string::npos, std::string("the error for '") + text + "' does not name the variable: " + message);
        require(message.find(std::string("'") + text + "'") != std::string::npos, std::string("the error for '") + text + "' does not quote it: " + message);
    }
}

void zeroLimitTests() {
    bool refused = false;
    try {
        FramePacer pacer(0);
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "a pacer for 0 frames per second was constructed");
}

}

int main() {
    try {
        deadlineArithmeticTests();
        overshootDoesNotAccumulateTests();
        stallDoesNotBurstTests();
        lateFrameIsMadeUpTests();
        slowGameIsNeverDelayedTests();
        zeroLimitTests();
        parseAcceptedTests();
        parseDisabledTests();
        parseRefusedTests();
        paceTests();
        paceOvershootTests();
        paceAbandonTests();
        paceSlowGameTests();
        paceHighLimitTests();
        resetTests();
        std::puts("frame limit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
