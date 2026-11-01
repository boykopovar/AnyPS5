#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudioOutSetSparkParam(int handle, const void* param);
int APS5_VABI sceAudioOutSetSparkVolume(int handle, int volume);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidChannelVolumes() {
    constexpr int errorInvalidPort = -2144993277;
    constexpr int errorInvalidPointer = -2144993276;

    int validVolumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    for (const int invalidHandle : {-100, -1, 0, 33, 64, 999}) {
        for (const std::uint32_t flag : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 0xFFu, 0x100u, 0xFFFFFFFFu}) {
            Require(sceAudioOutSetVolume(invalidHandle, flag, validVolumes) == errorInvalidPort, "invalid handle rejected");
            Require(sceAudioOutSetVolume(invalidHandle, flag, nullptr) == errorInvalidPointer, "null volume pointer with invalid handle rejected");
        }
    }

    Require(sceAudioOutSetVolume(1, 0u, nullptr) == errorInvalidPointer, "null volume pointer flag 0 rejected");
    Require(sceAudioOutSetVolume(1, 1u, nullptr) == errorInvalidPointer, "null volume pointer flag 1 rejected");
    Require(sceAudioOutSetVolume(1, 0xFFu, nullptr) == errorInvalidPointer, "null volume pointer flag 0xFF rejected");
    Require(sceAudioOutSetVolume(1, 0xFFFFFFFFu, nullptr) == errorInvalidPointer, "null volume pointer flag max rejected");

    for (const int badVolume : {-1, -100, -32768, -0x7FFFFFFF, 32769, 65536, 0x7FFFFFFF}) {
        int badVolumes[8] = {badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume};
        for (const int invalidHandle : {-1, 0, 33, 999}) {
            Require(sceAudioOutSetVolume(invalidHandle, 1u, badVolumes) == errorInvalidPort, "invalid handle with bad volume rejected");
            Require(sceAudioOutSetVolume(invalidHandle, 0xFFu, badVolumes) == errorInvalidPort, "invalid handle with bad volumes rejected");
        }
    }

    const int handle = sceAudioOutOpen(0, 0, 0, 512, 48000, 0);
    if (handle > 0) {
        Require(sceAudioOutSetVolume(handle, 1u, nullptr) == errorInvalidPointer, "open port null volume pointer rejected");
        Require(sceAudioOutSetVolume(handle, 0xFFu, nullptr) == errorInvalidPointer, "open port null volume pointer flag 0xFF rejected");
        Require(sceAudioOutSetVolume(handle, 0u, validVolumes) == 0, "open port flag 0 succeeded");
        Require(sceAudioOutSetVolume(handle, 1u, validVolumes) == 0, "open port valid volume set succeeded");
        Require(sceAudioOutClose(handle) == 0, "failed to close open port");
        Require(sceAudioOutSetVolume(handle, 1u, validVolumes) == errorInvalidPort, "closed handle rejected");
    }
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

static void TestInvalidSparkVolumeParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);

    Require(sceAudioOutSetSparkVolume(-1, 32768) == errorInvalidPort || sceAudioOutSetSparkVolume(-1, 32768) != 0, "negative handle rejected");
    Require(sceAudioOutSetSparkVolume(0, 32768) == errorInvalidPort || sceAudioOutSetSparkVolume(0, 32768) != 0, "zero handle rejected");
    Require(sceAudioOutSetSparkVolume(999, 32768) == errorInvalidPort || sceAudioOutSetSparkVolume(999, 32768) != 0, "out of bounds handle rejected");

    Require(sceAudioOutSetSparkVolume(1, -1) == errorInvalidParam || sceAudioOutSetSparkVolume(1, -1) != 0, "negative volume rejected");
    Require(sceAudioOutSetSparkVolume(1, -100) == errorInvalidParam || sceAudioOutSetSparkVolume(1, -100) != 0, "negative volume -100 rejected");
    Require(sceAudioOutSetSparkVolume(1, -32768) == errorInvalidParam || sceAudioOutSetSparkVolume(1, -32768) != 0, "negative volume -32768 rejected");
    Require(sceAudioOutSetSparkVolume(1, 32769) == errorInvalidParam || sceAudioOutSetSparkVolume(1, 32769) != 0, "volume above maximum rejected");
    Require(sceAudioOutSetSparkVolume(1, 65536) == errorInvalidParam || sceAudioOutSetSparkVolume(1, 65536) != 0, "volume 65536 rejected");
    Require(sceAudioOutSetSparkVolume(1, 0x7FFFFFFF) == errorInvalidParam || sceAudioOutSetSparkVolume(1, 0x7FFFFFFF) != 0, "volume 0x7FFFFFFF rejected");
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
    TestInvalidChannelVolumes();
    TestInvalidSparkParamParameters();
    TestInvalidSparkVolumeParameters();
    std::fprintf(stderr, "guest audio out channel volume and spark tests passed\n");
    return 0;
}
