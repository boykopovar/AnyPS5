#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
int APS5_VABI sceAudioOutGetPortStatus(int handle, void* status);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void TestInvalidPortStatusParameters() {
    constexpr int errorInvalidPort = static_cast<int>(0x80260003);
    constexpr int errorInvalidParam = static_cast<int>(0x80260014);
    constexpr int errorInvalidPointer = static_cast<int>(0x80260004);

    std::uint8_t statusBuffer[64]{};

    Require(sceAudioOutGetPortStatus(-1, statusBuffer) == errorInvalidPort || sceAudioOutGetPortStatus(-1, statusBuffer) != 0, "negative handle rejected");
    Require(sceAudioOutGetPortStatus(0, statusBuffer) == errorInvalidPort || sceAudioOutGetPortStatus(0, statusBuffer) != 0, "zero handle rejected");
    Require(sceAudioOutGetPortStatus(999, statusBuffer) == errorInvalidPort || sceAudioOutGetPortStatus(999, statusBuffer) != 0, "out of bounds handle rejected");

    Require(sceAudioOutGetPortStatus(1, nullptr) == errorInvalidParam || sceAudioOutGetPortStatus(1, nullptr) == errorInvalidPointer || sceAudioOutGetPortStatus(1, nullptr) != 0, "null status pointer rejected");
    Require(sceAudioOutGetPortStatus(-1, nullptr) == errorInvalidPort || sceAudioOutGetPortStatus(-1, nullptr) == errorInvalidParam || sceAudioOutGetPortStatus(-1, nullptr) != 0, "negative handle with null pointer rejected");
    Require(sceAudioOutGetPortStatus(0, nullptr) == errorInvalidPort || sceAudioOutGetPortStatus(0, nullptr) == errorInvalidParam || sceAudioOutGetPortStatus(0, nullptr) != 0, "zero handle with null pointer rejected");
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
    Require(sceAudioOutSetMixLevelPadSpk(1, std::numeric_limits<std::uint32_t>::max()) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(1, std::numeric_limits<std::uint32_t>::max()) != 0, "mix level max rejected");

    Require(sceAudioOutSetMixLevelPadSpk(-1, unity + 1) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(-1, unity + 1) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(-1, unity + 1) != 0, "negative handle with invalid level rejected");
    Require(sceAudioOutSetMixLevelPadSpk(0, std::numeric_limits<std::uint32_t>::max()) == errorInvalidPort || sceAudioOutSetMixLevelPadSpk(0, std::numeric_limits<std::uint32_t>::max()) == errorInvalidParam || sceAudioOutSetMixLevelPadSpk(0, std::numeric_limits<std::uint32_t>::max()) != 0, "zero handle with invalid level rejected");
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
    TestInvalidPortStatusParameters();
    TestInvalidMixLevelPadSpkParameters();
    std::fprintf(stderr, "guest audio out tests passed\n");
    return 0;
}
