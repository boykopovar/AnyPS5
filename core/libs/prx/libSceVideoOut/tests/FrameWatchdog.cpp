#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

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

}

int main() {
    Values();
    return 0;
}
