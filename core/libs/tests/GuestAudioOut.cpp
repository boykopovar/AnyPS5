#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudioOutSetPortVolume(int handle, int* volume);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidPortVolumeParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);
    constexpr int errorInvalidPointer = static_cast<int>(0x80260004);

    int volumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    Require(sceAudioOutSetPortVolume(-1, volumes) == errorInvalidPort || sceAudioOutSetPortVolume(-1, volumes) != 0, "negative handle rejected");
    Require(sceAudioOutSetPortVolume(0, volumes) == errorInvalidPort || sceAudioOutSetPortVolume(0, volumes) != 0, "zero handle rejected");
    Require(sceAudioOutSetPortVolume(999, volumes) == errorInvalidPort || sceAudioOutSetPortVolume(999, volumes) != 0, "out of bounds handle rejected");

    Require(sceAudioOutSetPortVolume(1, nullptr) == errorInvalidParam || sceAudioOutSetPortVolume(1, nullptr) == errorInvalidPointer || sceAudioOutSetPortVolume(1, nullptr) != 0, "null volume pointer rejected");
    Require(sceAudioOutSetPortVolume(-1, nullptr) == errorInvalidPort || sceAudioOutSetPortVolume(-1, nullptr) == errorInvalidParam || sceAudioOutSetPortVolume(-1, nullptr) != 0, "negative handle with null pointer rejected");
    Require(sceAudioOutSetPortVolume(0, nullptr) == errorInvalidPort || sceAudioOutSetPortVolume(0, nullptr) == errorInvalidParam || sceAudioOutSetPortVolume(0, nullptr) != 0, "zero handle with null pointer rejected");
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
    TestInvalidPortVolumeParameters();
    std::fprintf(stderr, "guest audio out port volume tests passed\n");
    return 0;
}
