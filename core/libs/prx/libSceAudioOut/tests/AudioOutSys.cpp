#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

static constexpr int SYSTEM_USER = 0xFF;

static bool failed = false;

static void Require(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "failed: %s\n", what);
        failed = true;
    }
}

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    Require(Throws([] { sceAudioOutSysOpen(SYSTEM_USER, 0); }), "mode 0 throws: linear PCM opens through the Ex path");
    Require(Throws([] { sceAudioOutSysOpen(SYSTEM_USER, 6); }), "mode 6 throws");

    const int truehd = sceAudioOutSysOpen(SYSTEM_USER, 5);
    Require(truehd > 0, "mode 5 opens a TrueHD MAT port");
    const std::vector<std::int16_t> grain(1024 * 8, 0x1234);
    Require(sceAudioOutOutput(truehd, grain.data()) == 1024, "a mode 5 port takes 1024 eight-channel frames");
    Require(sceAudioOutOutput(truehd, nullptr) == 1024, "a mode 5 port drains");

    const int second = sceAudioOutSysOpen(SYSTEM_USER, 5);
    Require(second > 0 && second != truehd, "a second mode 5 port opens");

    Require(sceAudioOutSysConfigureOutput(1, 0, 5, 1, 0) == 0, "the HDMI MAT switch is accepted");
    Require(Throws([] { sceAudioOutSysConfigureOutput(0, 0, 5, 1, 0); }), "another output type throws");
    Require(Throws([] { sceAudioOutSysConfigureOutput(1, 1, 5, 1, 0); }), "nonzero flags throw");
    Require(Throws([] { sceAudioOutSysConfigureOutput(1, 0, 4, 1, 0); }), "another mode throws");
    Require(Throws([] { sceAudioOutSysConfigureOutput(1, 0, 5, 2, 0); }), "another target throws");

    int mainOutput = -1;
    Require(!Throws([&mainOutput] { mainOutput = sceAudioOutSetMainOutput(); }), "sceAudioOutSetMainOutput does not throw");
    Require(mainOutput == 0, "sceAudioOutSetMainOutput returns 0");

    Require(sceAudioOutSysClose(truehd) == 0, "close the first Sys port");
    Require(Throws([truehd] { sceAudioOutSysClose(truehd); }), "a closed Sys port throws");
    const int ex = sceAudioOutExOpen(SYSTEM_USER, 0);
    Require(ex > 0, "an Ex port opens");
    Require(Throws([ex] { sceAudioOutSysClose(ex); }), "an Ex port is not a Sys port");
    Require(Throws([] { sceAudioOutSysClose(0); }), "handle 0 is not a Sys port");
    Require(Throws([] { sceAudioOutSysClose(1000); }), "an out-of-range handle is not a Sys port");
    Require(sceAudioOutSysClose(second) == 0, "close the second Sys port");
    return failed ? 1 : 0;
}
