#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudioOutSetMasterVolume(int handle, int volume);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidMasterVolumeParameters() {
    constexpr int errorInvalidHandle = -2144993277;
    constexpr int errorInvalidVolume = -2144993260;

    Require(sceAudioOutSetMasterVolume(-1, 32768) == errorInvalidHandle || sceAudioOutSetMasterVolume(-1, 32768) != 0, "negative handle rejected");
    Require(sceAudioOutSetMasterVolume(0, 32768) == errorInvalidHandle || sceAudioOutSetMasterVolume(0, 32768) != 0, "zero handle rejected");
    Require(sceAudioOutSetMasterVolume(999, 32768) == errorInvalidHandle || sceAudioOutSetMasterVolume(999, 32768) != 0, "out of bounds handle rejected");

    Require(sceAudioOutSetMasterVolume(1, -1) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, -1) != 0, "negative volume rejected");
    Require(sceAudioOutSetMasterVolume(1, -100) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, -100) != 0, "negative volume -100 rejected");
    Require(sceAudioOutSetMasterVolume(1, -32768) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, -32768) != 0, "negative volume -32768 rejected");
    Require(sceAudioOutSetMasterVolume(1, 32769) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, 32769) != 0, "volume above maximum rejected");
    Require(sceAudioOutSetMasterVolume(1, 65536) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, 65536) != 0, "volume 65536 rejected");
    Require(sceAudioOutSetMasterVolume(1, 0x7FFFFFFF) == errorInvalidVolume || sceAudioOutSetMasterVolume(1, 0x7FFFFFFF) != 0, "volume 0x7FFFFFFF rejected");
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
    TestInvalidMasterVolumeParameters();
    std::fprintf(stderr, "guest audio out master volume tests passed\n");
    return 0;
}
