#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    constexpr int INVALID_PORT = -2144993277;
    constexpr int INVALID_POINTER = -2144993276;

    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(-1, 1, vol) == INVALID_PORT, "invalid handle returns invalid port");
    Require(sceAudioOutSetVolume(999, 1, vol) == INVALID_PORT, "out of range handle returns invalid port");

    Require(sceAudioOutSetVolume(-1, 1, nullptr) == INVALID_POINTER, "null volume pointer returns invalid pointer");

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0, "open valid port");
    Require(sceAudioOutSetVolume(port, 1, nullptr) == INVALID_POINTER, "null volume pointer on valid port returns invalid pointer");
    Require(sceAudioOutClose(port) == 0, "close port");

    std::puts("audio out set volume validation: PASS");
}
