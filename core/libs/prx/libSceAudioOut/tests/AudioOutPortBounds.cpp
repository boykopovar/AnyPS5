#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    AudioOutPortState state;

    Require(sceAudioOutSetVolume(0, 1, nullptr) == -2144993276, "null volume pointer error");
    Require(sceAudioOutSetVolume(-1, 1, vol) == -2144993277, "invalid negative handle error");
    Require(sceAudioOutSetVolume(999, 1, vol) == -2144993277, "invalid out of range handle error");

    Require(sceAudioOutGetPortState(0, nullptr) == -2144993276, "null state pointer error");
    Require(sceAudioOutGetPortState(-1, &state) == -2144993277, "invalid negative handle state error");
    Require(sceAudioOutGetPortState(999, &state) == -2144993277, "invalid out of range handle state error");

    std::puts("audio out port bounds: PASS");
}
