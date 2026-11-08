#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "frame watchdog test failed: %s\n", what);
    std::exit(1);
}

void Values() {
    std::uint32_t seconds = 7;
    std::string error;
    Require(frame_watchdog::ParseHangSeconds(nullptr, seconds, error) && seconds == 0 && error.empty(), "an unset value is not off");
    Require(frame_watchdog::ParseHangSeconds("", seconds, error) && seconds == 0, "an empty value is not off");
    Require(frame_watchdog::ParseHangSeconds("0", seconds, error) && seconds == 0, "zero is not off");
    Require(frame_watchdog::ParseHangSeconds("1", seconds, error) && seconds == 1, "one second was refused");
    Require(frame_watchdog::ParseHangSeconds("86400", seconds, error) && seconds == 86400, "the largest value was refused");
    for (const char* text : {"-1", "+3", " 3", "3s", "86401", "99999999999", "1.5"}) {
        error.clear();
        Require(!frame_watchdog::ParseHangSeconds(text, seconds, error) && error.find("ANYPS5_HANG_SECONDS") != std::string::npos, text);
    }
}

void ReportsOncePerHang() {
    constexpr std::int64_t Threshold = 1000;
    frame_watchdog::HangState state;
    Require(!frame_watchdog::HangReportDue(state, 100, 100 + Threshold - 1, Threshold), "reported before the threshold");
    Require(frame_watchdog::HangReportDue(state, 100, 100 + Threshold, Threshold), "no report at the threshold");
    Require(!frame_watchdog::HangReportDue(state, 100, 100 + 5 * Threshold, Threshold), "the same hang was reported again");
    Require(!frame_watchdog::HangReportDue(state, 100, 100 + 9 * Threshold, Threshold), "the same hang was reported on later checks");
    Require(!frame_watchdog::HangReportDue(state, 500, 500 + Threshold - 1, Threshold), "a new frame did not reset the hang");
    Require(frame_watchdog::HangReportDue(state, 500, 500 + Threshold, Threshold), "a second hang was not reported");
    Require(!frame_watchdog::HangReportDue(state, 500, 500 + 2 * Threshold, Threshold), "the second hang was reported twice");
}

void StartAndStop() {
    frame_watchdog::Start(1);
    for (int index = 0; index < 15; ++index) {
        frame_watchdog::Beat();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    frame_watchdog::Stop();
    frame_watchdog::Stop();
    frame_watchdog::Start(0);
    frame_watchdog::Stop();
}

}

int main() {
    Values();
    ReportsOncePerHang();
    StartAndStop();
    return 0;
}
