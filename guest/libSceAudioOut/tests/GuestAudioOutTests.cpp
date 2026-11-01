#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void TestInvalidChannelCountsInOpen() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr std::uint32_t freq = 48000;

    for (const int type : {0, 1, 2, 3, 4, 10, 126, 127}) {
        for (const std::uint32_t len : {256u, 512u, 1024u, 2048u}) {
            for (const std::uint32_t badChannelFormat : {8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
                                                         16u, 18u, 24u, 32u, 48u, 64u, 96u,
                                                         128u, 192u, 254u, 255u}) {
                Require(sceAudioOutOpen(userId, type, 0, len, freq, badChannelFormat) == errorInvalidFormat,
                        "invalid channel format rejected");
                Require(sceAudioOutOpen(userId + 1, type, 0, len, freq, badChannelFormat) == errorInvalidFormat,
                        "secondary user invalid channel format rejected");
                Require(sceAudioOutOpen(0, type, 0, len, freq, badChannelFormat) == errorInvalidFormat,
                        "user 0 invalid channel format rejected");
                Require(sceAudioOutOpen(-1, type, 0, len, freq, badChannelFormat) == errorInvalidFormat,
                        "user -1 invalid channel format rejected");
            }
        }
    }
}

void TestInvalidChannelConfigsWithFormatFlags() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr std::uint32_t len = 256;
    constexpr std::uint32_t freq = 48000;

    for (const int type : {0, 1, 2, 3, 4, 10, 126, 127}) {
        for (const std::uint32_t badParam : {
                 0x108u, 0x109u, 0x110u, 0x118u, 0x120u, 0x1FFu,
                 0x208u, 0x209u, 0x210u, 0x218u, 0x220u, 0x2FFu,
                 0x1008u, 0x1010u, 0x1018u, 0x1020u,
                 0x10008u, 0x10010u, 0x10018u, 0x10020u,
                 0x80000008u, 0x80000010u, 0x80000018u, 0x80000020u, 0x800000FFu,
                 0xFFFFFF08u, 0xFFFFFF10u, 0xFFFFFF18u, 0xFFFFFF20u, 0xFFFFFFFFu}) {
            Require(sceAudioOutOpen(userId, type, 0, len, freq, badParam) == errorInvalidFormat,
                    "invalid channel config with flags rejected");
        }
    }
}

void TestUnsupportedVibrationChannelLayouts() {
    constexpr int userId = 0x10000000;
    constexpr int vibrationType = 10;
    constexpr std::uint32_t freq = 48000;

    for (const std::uint32_t badLayoutFormat : {2u, 5u, 6u, 7u}) {
        for (const std::uint32_t len : {256u, 512u, 1024u}) {
            for (const int user : {userId, userId + 1, 0, -1}) {
                bool threw = false;
                try {
                    static_cast<void>(sceAudioOutOpen(user, vibrationType, 0, len, freq, badLayoutFormat));
                } catch (const std::runtime_error&) {
                    threw = true;
                }
                Require(threw, "multichannel vibration layout above stereo threw runtime_error");
            }
        }
    }
}

void TestUnsupportedIndexInOpen() {
    constexpr int userId = 0x10000000;
    constexpr std::uint32_t len = 256;
    constexpr std::uint32_t freq = 48000;
    constexpr std::uint32_t stereoFormat = 1;

    for (const int badIndex : {-1, 1, 2, 5, 100, 999}) {
        bool threw = false;
        try {
            static_cast<void>(sceAudioOutOpen(userId, 0, badIndex, len, freq, stereoFormat));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "nonzero open index threw runtime_error");
    }
}

void TestChannelConfigValidationPrecedence() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidType = -2144993270;
    constexpr int errorInvalidLen = -2144993274;
    constexpr int errorInvalidFreq = -2144993272;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr std::uint32_t badChannelFormat = 16u;

    for (const int invalidType : {-100, -1, 5, 6, 7, 8, 9, 11, 125, 128, 999}) {
        Require(sceAudioOutOpen(userId, invalidType, 0, 256, 48000, badChannelFormat) == errorInvalidType,
                "invalid port type precedence rejected");
        Require(sceAudioOutOpen(userId, invalidType, 0, 256, 48000, 2) == errorInvalidType,
                "invalid port type precedence for 8ch layout rejected");
    }

    Require(sceAudioOutOpen(userId, 0, 0, 0, 48000, badChannelFormat) == errorInvalidLen,
            "zero length precedence rejected");
    Require(sceAudioOutOpen(userId, 1, 0, 0, 48000, badChannelFormat) == errorInvalidLen,
            "bgm port zero length precedence rejected");
    Require(sceAudioOutOpen(userId, 10, 0, 0, 48000, 2) == errorInvalidLen,
            "vibration port zero length precedence for 8ch rejected");

    Require(sceAudioOutOpen(userId, 0, 0, 256, 0, badChannelFormat) == errorInvalidFreq,
            "zero frequency precedence rejected");
    Require(sceAudioOutOpen(userId, 1, 0, 256, 0, badChannelFormat) == errorInvalidFreq,
            "bgm port zero frequency precedence rejected");
    Require(sceAudioOutOpen(userId, 10, 0, 256, 0, 2) == errorInvalidFreq,
            "vibration port zero frequency precedence for 8ch rejected");

    Require(sceAudioOutOpen(userId, 0, 0, 256, 48000, badChannelFormat) == errorInvalidFormat,
            "valid type, len, freq with bad channel format returns invalid format");
}

}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" unsigned int SDL_WasInit(unsigned int) { return 0x00000010u; }
extern "C" int SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDL_GetAudioDeviceName(int, int) { return "test"; }
extern "C" int SDL_GetAudioDeviceSpec(int, int, void*) { return 0; }
extern "C" const char* SDL_GetCurrentAudioDriver() { return "dummy"; }
extern "C" const char* SDL_GetError() { return "error"; }
extern "C" unsigned int SDL_OpenAudioDevice(const char*, int, const void*, void*, int) { return 1; }
extern "C" void SDL_CloseAudioDevice(unsigned int) {}
extern "C" void SDL_QueueAudio(unsigned int, const void*, unsigned int) {}
extern "C" void SDL_ClearQueuedAudio(unsigned int) {}
extern "C" unsigned int SDL_GetQueuedAudioSize(unsigned int) { return 0; }
extern "C" void SDL_PauseAudioDevice(unsigned int, int) {}

int main() {
    TestInvalidChannelCountsInOpen();
    TestInvalidChannelConfigsWithFormatFlags();
    TestUnsupportedVibrationChannelLayouts();
    TestUnsupportedIndexInOpen();
    TestChannelConfigValidationPrecedence();
    std::fprintf(stderr, "guest audio out channel config bounds tests passed\n");
    return 0;
}
