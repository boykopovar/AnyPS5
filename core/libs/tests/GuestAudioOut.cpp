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

static void TestInvalidMixLevelPadSpkParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);
    constexpr std::uint32_t unity = 32768;

    Require(sceAudioOutSetMixLevelPadSpk(-1, unity) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(-1, unity) != 0, "negative handle rejected");
    Require(sceAudioOutSetMixLevelPadSpk(0, unity) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(0, unity) != 0, "zero handle rejected");
    Require(sceAudioOutSetMixLevelPadSpk(999, unity) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(999, unity) != 0, "out of bounds handle rejected");

    Require(sceAudioOutSetMixLevelPadSpk(1, unity + 1) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(1, unity + 1) != 0, "mix level above maximum rejected");
    Require(sceAudioOutSetMixLevelPadSpk(1, 65536) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(1, 65536) != 0, "mix level 65536 rejected");
    Require(sceAudioOutSetMixLevelPadSpk(1, UINT32_MAX) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(1, UINT32_MAX) != 0, "mix level UINT32_MAX rejected");

    Require(sceAudioOutSetMixLevelPadSpk(-1, unity + 1) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(-1, unity + 1) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(-1, unity + 1) != 0, "negative handle with invalid level rejected");
    Require(sceAudioOutSetMixLevelPadSpk(0, UINT32_MAX) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(0, UINT32_MAX) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(0, UINT32_MAX) != 0, "zero handle with invalid level rejected");
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
    TestInvalidMixLevelPadSpkParameters();
    std::fprintf(stderr, "guest audio out mix level pad spk tests passed\n");
    return 0;
}
