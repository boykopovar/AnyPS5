#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidVolumeParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);

    int validVolumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    Require(sceAudioOutSetVolume(-1, 0x1u, validVolumes) == errorInvalidPort || sceAudioOutSetVolume(-1, 0x1u, validVolumes) != 0, "negative handle rejected");
    Require(sceAudioOutSetVolume(0, 0x1u, validVolumes) == errorInvalidPort || sceAudioOutSetVolume(0, 0x1u, validVolumes) != 0, "zero handle rejected");
    Require(sceAudioOutSetVolume(999, 0x1u, validVolumes) == errorInvalidPort || sceAudioOutSetVolume(999, 0x1u, validVolumes) != 0, "out of bounds handle rejected");

    for (const int badVolume : {-1, -100, -32768, 32769, 65536, 0x7FFFFFFF}) {
        int badVolumes[8] = {badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume};
        Require(sceAudioOutSetVolume(1, 0x1u, badVolumes) == errorInvalidParam || sceAudioOutSetVolume(1, 0x1u, badVolumes) != 0, "out-of-range volume rejected");
        Require(sceAudioOutSetVolume(-1, 0x1u, badVolumes) == errorInvalidPort || sceAudioOutSetVolume(-1, 0x1u, badVolumes) == errorInvalidParam || sceAudioOutSetVolume(-1, 0x1u, badVolumes) != 0, "negative handle with bad volume rejected");
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
    TestInvalidVolumeParameters();
    std::fprintf(stderr, "guest audio out set volume tests passed\n");
    return 0;
}
