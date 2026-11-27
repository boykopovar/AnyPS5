#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

std::uint32_t seen = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_audio_haptics_merge: %s\n", message);
    std::abort();
}

void CheckOutOfRangeNoOp() {
    PadOutputState out{};
    for (int port : {-2, -1, PAD_MAX_CONTROLLERS, PAD_MAX_CONTROLLERS + 1}) {
        PadSubmitAudioHapticsForPort_nid_postfix(port, 100, 50);
        Require(!PadFetchOutput_nid_postfix(&seen, &out), "out-of-range port submission modified output sequence");
    }
}

void CheckZeroZeroNoOp() {
    PadOutputState out{};
    PadSubmitAudioHapticsForPort_nid_postfix(0, 0, 0);
    Require(!PadFetchOutput_nid_postfix(&seen, &out), "(0, 0) submission modified initial output sequence");
}

using Clock = std::chrono::steady_clock;

enum class Window { Merged, Reset, Either };

Window Classify(Clock::duration shortest, Clock::duration longest) {
    constexpr auto Length = std::chrono::milliseconds(80);
    if (longest < Length) return Window::Merged;
    if (shortest >= Length) return Window::Reset;
    return Window::Either;
}

bool Matches(std::uint8_t actual, std::uint8_t previous, std::uint8_t submitted, Window window) {
    const auto merged = std::max(previous, submitted);
    if (window == Window::Merged) return actual == merged;
    if (window == Window::Reset) return actual == submitted;
    return actual == merged || actual == submitted;
}

void CheckMergeWithinWindow() {
    const std::uint32_t baseSequence = seen;
    PadOutputState first{};
    const auto beforeFirst = Clock::now();
    PadSubmitAudioHapticsForPort_nid_postfix(0, 40, 120);
    const auto afterFirst = Clock::now();
    Require(PadFetchOutput_nid_postfix(&seen, &first), "first submission updated output");
    Require(first.sequence == baseSequence + 1, "first submission incremented sequence once");
    Require(seen == first.sequence, "seen sequence tracked first submission");
    Require(first.audioVibrationLarge == 40, "large is 40 after first submission");
    Require(first.audioVibrationSmall == 120, "small is 120 after first submission");

    PadOutputState second{};
    const auto beforeSecond = Clock::now();
    PadSubmitAudioHapticsForPort_nid_postfix(0, 90, 50);
    const auto afterSecond = Clock::now();
    Require(PadFetchOutput_nid_postfix(&seen, &second), "second submission updated output");
    Require(second.sequence == baseSequence + 2, "second submission incremented sequence once");
    const auto secondWindow = Classify(beforeSecond - afterFirst, afterSecond - beforeFirst);
    Require(Matches(second.audioVibrationLarge, 40, 90, secondWindow), "second large follows the 80 ms merge window");
    Require(Matches(second.audioVibrationSmall, 120, 50, secondWindow), "second small follows the 80 ms merge window");

    PadSubmitAudioHapticsForPort_nid_postfix(0, 0, 0);
    PadOutputState afterZero{};
    Require(!PadFetchOutput_nid_postfix(&seen, &afterZero), "(0, 0) after accepted submission incremented sequence");

    PadSubmitAudioHapticsForPort_nid_postfix(-1, 200, 200);
    Require(!PadFetchOutput_nid_postfix(&seen, &afterZero), "out-of-range port after accepted submission incremented sequence");

    PadOutputState third{};
    const auto beforeThird = Clock::now();
    PadSubmitAudioHapticsForPort_nid_postfix(0, 110, 80);
    const auto afterThird = Clock::now();
    Require(PadFetchOutput_nid_postfix(&seen, &third), "third submission updated output");
    Require(third.sequence == baseSequence + 3, "third submission incremented sequence to +3");
    const auto thirdWindow = Classify(beforeThird - afterSecond, afterThird - beforeSecond);
    Require(Matches(third.audioVibrationLarge, second.audioVibrationLarge, 110, thirdWindow), "third large follows the 80 ms merge window");
    Require(Matches(third.audioVibrationSmall, second.audioVibrationSmall, 80, thirdWindow), "third small follows the 80 ms merge window");
}

}

int main() {
    Require(scePadInit_nid_postfix() == 0, "scePadInit succeeded");
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0, "scePadOpen succeeded");

    PadOutputState initial{};
    static_cast<void>(PadFetchOutput_nid_postfix(&seen, &initial));

    CheckOutOfRangeNoOp();
    CheckZeroZeroNoOp();
    CheckMergeWithinWindow();

    std::puts("guest pad audio haptics merge tests passed");
    return 0;
}
