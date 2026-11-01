#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void TestInvalidAudioOutputFormat() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr std::uint32_t len = 256;
    constexpr std::uint32_t freq = 48000;

    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 8) == errorInvalidFormat, "format 8 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 9) == errorInvalidFormat, "format 9 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 10) == errorInvalidFormat, "format 10 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 15) == errorInvalidFormat, "format 15 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 16) == errorInvalidFormat, "format 16 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 99) == errorInvalidFormat, "format 99 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 128) == errorInvalidFormat, "format 128 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 255) == errorInvalidFormat, "format 255 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 0x108) == errorInvalidFormat, "format 0x108 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 0x1FF) == errorInvalidFormat, "format 0x1FF rejected");
    Require(sceAudioOutOpen(userId, 0, 0, len, freq, 0xFFFFFFFFu) == errorInvalidFormat, "format 0xFFFFFFFF rejected");

    Require(sceAudioOutOpen(userId, 1, 0, len, freq, 8) == errorInvalidFormat, "bgm port format 8 rejected");
    Require(sceAudioOutOpen(userId, 2, 0, len, freq, 8) == errorInvalidFormat, "voice port format 8 rejected");
    Require(sceAudioOutOpen(userId, 3, 0, len, freq, 8) == errorInvalidFormat, "personal port format 8 rejected");
    Require(sceAudioOutOpen(userId, 4, 0, len, freq, 8) == errorInvalidFormat, "padspk port format 8 rejected");
    Require(sceAudioOutOpen(userId, 10, 0, len, freq, 8) == errorInvalidFormat, "vibration port format 8 rejected");
    Require(sceAudioOutOpen(userId, 126, 0, len, freq, 8) == errorInvalidFormat, "audio3d port format 8 rejected");
    Require(sceAudioOutOpen(userId, 127, 0, len, freq, 8) == errorInvalidFormat, "aux port format 8 rejected");
}

}

int main() {
    TestInvalidAudioOutputFormat();
    return 0;
}
