#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>

static bool failed = false;

static void Require(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "failed: %s\n", what);
        failed = true;
    }
}

int main() {
    constexpr int userId = 0x10000000;
    constexpr int invalidType = -2144993270;
    constexpr int invalidFormat = static_cast<int>(0x80260007);

    Require(sceAudioOutOpen(userId, -1, 0, 256, 48000, 1) == invalidType, "negative port type is rejected");
    Require(sceAudioOutOpen(userId, 3, 0, 256, 48000, 1) == invalidType, "port type 3 (personal) is rejected");
    Require(sceAudioOutOpen(userId, 5, 0, 256, 48000, 1) == invalidType, "port type 5 is rejected");

    Require(sceAudioOutOpen(userId, 0, 0, 256, 48000, 99) == invalidFormat, "invalid channel/format parameter is rejected");

    return failed ? 1 : 0;
}
