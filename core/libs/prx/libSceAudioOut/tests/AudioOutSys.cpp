#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

static constexpr int SYSTEM_USER = 0xFF;

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
    Expect(Throws([] { sceAudioOutSysOpen(SYSTEM_USER, 0); }), "mode 0 throws: linear PCM opens through the Ex path");
    Expect(Throws([] { sceAudioOutSysOpen(SYSTEM_USER, 6); }), "mode 6 throws");

    const int truehd = sceAudioOutSysOpen(SYSTEM_USER, 5);
    Expect(truehd > 0, "mode 5 opens a TrueHD MAT port");
    const std::vector<std::int16_t> grain(1024 * 8, 0x1234);
    Expect(sceAudioOutOutput(truehd, grain.data()) == 1024, "a mode 5 port takes 1024 eight-channel frames");
    Expect(sceAudioOutOutput(truehd, nullptr) == 1024, "a mode 5 port drains");

    const int second = sceAudioOutSysOpen(SYSTEM_USER, 5);
    Expect(second > 0 && second != truehd, "a second mode 5 port opens");

    Expect(sceAudioOutSysConfigureOutput(1, 0, 5, 1, 0) == 0, "the HDMI MAT switch is accepted");
    Expect(Throws([] { sceAudioOutSysConfigureOutput(0, 0, 5, 1, 0); }), "another output type throws");
    Expect(Throws([] { sceAudioOutSysConfigureOutput(1, 1, 5, 1, 0); }), "nonzero flags throw");
    Expect(Throws([] { sceAudioOutSysConfigureOutput(1, 0, 4, 1, 0); }), "another mode throws");
    Expect(Throws([] { sceAudioOutSysConfigureOutput(1, 0, 5, 2, 0); }), "another target throws");

    int mainOutput = -1;
    Expect(!Throws([&mainOutput] { mainOutput = sceAudioOutSetMainOutput(); }), "sceAudioOutSetMainOutput does not throw");
    Expect(mainOutput == 0, "sceAudioOutSetMainOutput returns 0");

    Expect(sceAudioOutSysClose(truehd) == 0, "close the first Sys port");
    Expect(Throws([truehd] { sceAudioOutSysClose(truehd); }), "a closed Sys port throws");
    const int ex = sceAudioOutExOpen(SYSTEM_USER, 0);
    Expect(ex > 0, "an Ex port opens");
    Expect(Throws([ex] { sceAudioOutSysClose(ex); }), "an Ex port is not a Sys port");
    Expect(Throws([] { sceAudioOutSysClose(0); }), "handle 0 is not a Sys port");
    Expect(Throws([] { sceAudioOutSysClose(1000); }), "an out-of-range handle is not a Sys port");
    Expect(sceAudioOutSysClose(second) == 0, "close the second Sys port");
    return Failures() == 0 ? 0 : 1;
}
