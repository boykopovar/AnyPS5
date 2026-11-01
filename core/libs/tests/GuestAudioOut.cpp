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

static void TestInvalidPortStateParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);

    AudioOutPortState state{};

    Require(sceAudioOutGetPortState(-1, &state) == errorInvalidPort || sceAudioOutGetPortState(-1, &state) != 0, "negative handle rejected");
    Require(sceAudioOutGetPortState(0, &state) == errorInvalidPort || sceAudioOutGetPortState(0, &state) != 0, "zero handle rejected");
    Require(sceAudioOutGetPortState(9999, &state) == errorInvalidPort || sceAudioOutGetPortState(9999, &state) != 0, "out of bounds handle rejected");
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
    TestInvalidPortStateParameters();
    std::fprintf(stderr, "guest audio out get port state tests passed\n");
    return 0;
}
