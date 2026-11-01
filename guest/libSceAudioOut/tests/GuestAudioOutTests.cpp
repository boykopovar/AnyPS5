#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
int APS5_VABI sceAudioOutSetSparkParam(int handle, const void* param);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidAudioOutputFormat() {
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

static void TestInvalidSystemStateParameters() {
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);
    constexpr int errorInvalidPointer = static_cast<int>(0x80260004);

    Require(sceAudioOutGetSystemState(nullptr) == errorInvalidParam || sceAudioOutGetSystemState(nullptr) == errorInvalidPointer || sceAudioOutGetSystemState(nullptr) != 0, "null system state pointer rejected");
}

static void TestInvalidSparkParamParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);
    constexpr int errorInvalidPointer = static_cast<int>(0x80260004);

    std::uint8_t paramBuffer[64]{};

    Require(sceAudioOutSetSparkParam(-1, paramBuffer) == errorInvalidPort || sceAudioOutSetSparkParam(-1, paramBuffer) != 0, "negative handle rejected");
    Require(sceAudioOutSetSparkParam(0, paramBuffer) == errorInvalidPort || sceAudioOutSetSparkParam(0, paramBuffer) != 0, "zero handle rejected");
    Require(sceAudioOutSetSparkParam(999, paramBuffer) == errorInvalidPort || sceAudioOutSetSparkParam(999, paramBuffer) != 0, "out of bounds handle rejected");

    Require(sceAudioOutSetSparkParam(1, nullptr) == errorInvalidParam || sceAudioOutSetSparkParam(1, nullptr) == errorInvalidPointer || sceAudioOutSetSparkParam(1, nullptr) != 0, "null param pointer rejected");
    Require(sceAudioOutSetSparkParam(-1, nullptr) == errorInvalidPort || sceAudioOutSetSparkParam(-1, nullptr) == errorInvalidParam || sceAudioOutSetSparkParam(-1, nullptr) != 0, "negative handle with null pointer rejected");
    Require(sceAudioOutSetSparkParam(0, nullptr) == errorInvalidPort || sceAudioOutSetSparkParam(0, nullptr) == errorInvalidParam || sceAudioOutSetSparkParam(0, nullptr) != 0, "zero handle with null pointer rejected");
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
    TestInvalidAudioOutputFormat();
    TestInvalidSystemStateParameters();
    TestInvalidSparkParamParameters();
    std::fprintf(stderr, "guest audio out tests passed\n");
    return 0;
}
