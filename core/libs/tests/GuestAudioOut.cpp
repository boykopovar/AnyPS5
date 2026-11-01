#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudioOutSetSparkVolume(int handle, int volume);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
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
    TestInvalidSparkVolumeParameters();
    std::fprintf(stderr, "guest audio out spark volume tests passed\n");
    return 0;
}
