#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestInvalidPortTypesAndOpenParams() {
    constexpr int userId = 0x10000000;
    constexpr int errorInvalidType = -2144993270;
    constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
    constexpr int errorInvalidLen = -2144993274;
    constexpr int errorInvalidFreq = -2144993272;

    Require(sceAudioOutOpen(userId, -1, 0, 256, 48000, 1) == errorInvalidType, "negative port type rejected");
    Require(sceAudioOutOpen(userId, 5, 0, 256, 48000, 1) == errorInvalidType, "port type 5 rejected");
    Require(sceAudioOutOpen(userId, 8, 0, 256, 48000, 1) == errorInvalidType, "port type 8 rejected");
    Require(sceAudioOutOpen(userId, 9, 0, 256, 48000, 1) == errorInvalidType, "port type 9 rejected");
    Require(sceAudioOutOpen(userId, 11, 0, 256, 48000, 1) == errorInvalidType, "port type 11 rejected");
    Require(sceAudioOutOpen(userId, 128, 0, 256, 48000, 1) == errorInvalidType, "port type 128 rejected");

    Require(sceAudioOutOpen(userId, 0, 0, 0, 48000, 1) == errorInvalidLen, "zero length rejected");
    Require(sceAudioOutOpen(userId, 0, 0, 256, 0, 1) == errorInvalidFreq, "zero frequency rejected");

    Require(sceAudioOutOpen(userId, 0, 0, 256, 48000, 8) == errorInvalidFormat, "format 8 rejected");
    Require(sceAudioOutOpen(userId, 0, 0, 256, 48000, 99) == errorInvalidFormat, "invalid channel format parameter rejected");
    Require(sceAudioOutOpen(userId, 0, 0, 256, 48000, 0xFF) == errorInvalidFormat, "format 0xFF rejected");
}

void TestVolumeParametersAndMixLevels() {
    constexpr int errorInvalidHandle = -2144993277;
    constexpr int errorInvalidPointer = -2144993276;
    constexpr int errorInvalidMixLevel = -2144993260;
    constexpr int errorInvalidPortType = -2144993270;

    int volumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    Require(sceAudioOutSetVolume(-1, 0x1u, volumes) == errorInvalidHandle, "negative handle rejected");
    Require(sceAudioOutSetVolume(0, 0x1u, volumes) == errorInvalidHandle, "zero handle rejected");
    Require(sceAudioOutSetVolume(999, 0x1u, volumes) == errorInvalidHandle, "out of bounds handle rejected");
    Require(sceAudioOutSetVolume(1, 0x1u, nullptr) == errorInvalidPointer, "null volume pointer rejected");

    Require(sceAudioOutSetMixLevelPadSpk(-1, 11626) == errorInvalidHandle, "negative handle in set mix level rejected");
    Require(sceAudioOutSetMixLevelPadSpk(0, 11626) == errorInvalidHandle, "zero handle in set mix level rejected");
    Require(sceAudioOutSetMixLevelPadSpk(999, 11626) == errorInvalidHandle, "out of bounds handle in set mix level rejected");

    const int handle = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(handle > 0, "sceAudioOutOpen valid port failed");

    Require(sceAudioOutSetVolume(handle, 0x1u, nullptr) == errorInvalidPointer, "null volume pointer on valid port rejected");
    Require(sceAudioOutSetVolume(handle, 0x1u, volumes) == 0, "valid volume set succeeded");

    Require(sceAudioOutSetMixLevelPadSpk(handle, 32769) == errorInvalidMixLevel, "mix level above max rejected");
    Require(sceAudioOutSetMixLevelPadSpk(handle, 11626) == errorInvalidPortType, "mix level on main port rejected as invalid port type");

    Require(sceAudioOutClose(handle) == 0, "sceAudioOutClose failed");
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

int main() try {
    TestInvalidPortTypesAndOpenParams();
    TestVolumeParametersAndMixLevels();
    std::puts("guest audio out open and volume tests passed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
