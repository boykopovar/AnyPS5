#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

static constexpr int SYSTEM_USER = 0xFF;
static constexpr int ERROR_EX_INVALID_MODE = static_cast<int>(0x80260015u);
static constexpr int ERROR_EX_INVALID_CONTROL = static_cast<int>(0x80268009u);

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
    Require(sceAudioOutExOpen(SYSTEM_USER, 5) == ERROR_EX_INVALID_MODE, "mode 5 (Linear PCM) is refused");
    Require(sceAudioOutExOpen(SYSTEM_USER, 7) == ERROR_EX_INVALID_MODE, "mode 7 is outside mask 0x61F");
    Require(sceAudioOutExOpen(SYSTEM_USER, 11) == ERROR_EX_INVALID_MODE, "mode 11 is above 10");

    const int ac3 = sceAudioOutExOpen(SYSTEM_USER, 0);
    Require(ac3 > 0, "mode 0 opens a port");
    const std::vector<std::int16_t> burst(256 * 2, 0x1234);
    Require(sceAudioOutOutput(ac3, burst.data()) == 256, "a mode 0 port takes 256 frames");
    Require(sceAudioOutOutput(ac3, nullptr) == 256, "a mode 0 port drains");

    const int eac3 = sceAudioOutExOpen(SYSTEM_USER, 4);
    Require(eac3 > 0 && eac3 != ac3, "mode 4 opens a second port");
    const std::vector<std::int16_t> wide(1024 * 2, 0);
    Require(sceAudioOutOutput(eac3, wide.data()) == 1024, "a mode 4 port takes 1024 frames");

    Require(sceAudioOutExConfigureOutput(1, 0, 0, 1, 0) == ERROR_EX_INVALID_CONTROL, "a nonzero first argument is refused");
    Require(sceAudioOutExConfigureOutput(0, 0, 0, 1, 0) == 0, "mode 0 to HDMI");
    Require(sceAudioOutExConfigureOutput(0, 0, 0, 0xFF, 0) == 0, "mode 0 to the default target");
    Require(sceAudioOutExConfigureOutput(0, 0, 0xFF, 0xFF, 0) == 0, "the reset");
    Require(Throws([] { sceAudioOutExConfigureOutput(0, 0, 11, 1, 0); }), "an unknown mode throws");
    Require(Throws([] { sceAudioOutExConfigureOutput(0, 0, 0, 4, 0); }), "an unknown target throws");
    Require(Throws([] { sceAudioOutExConfigureOutput(0, 1, 0, 1, 0); }), "nonzero flags throw");

    Require(sceAudioOutExClose(ac3) == 0, "close the mode 0 port");
    Require(Throws([ac3] { sceAudioOutExClose(ac3); }), "a closed port throws");
    Require(sceAudioOutExClose(eac3) == 0, "close the mode 4 port");
    return failed ? 1 : 0;
}
