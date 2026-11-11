#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SDL.h"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Main speakers"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec* spec) {
    if (spec != nullptr) {
        *spec = {};
        spec->channels = 2;
    }
    return 0;
}
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return "wasapi"; }
extern "C" const char* SDLCALL SDL_GetError() { return "test error"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    if (obtained != nullptr && desired != nullptr) {
        *obtained = *desired;
    }
    return 7;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32) { return 0; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) {}
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID) {}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID) { return SDL_AUDIO_PLAYING; }

int main() {
    constexpr int INVALID_PORT = -2144993277;
    constexpr int INVALID_POINTER = -2144993276;

    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(-1, 1, vol) == INVALID_PORT, "invalid handle returns invalid port");
    Require(sceAudioOutSetVolume(0, 1, vol) == INVALID_PORT, "zero handle returns invalid port");
    Require(sceAudioOutSetVolume(999, 1, vol) == INVALID_PORT, "out of range handle returns invalid port");

    Require(sceAudioOutSetVolume(-1, 1, nullptr) == INVALID_POINTER, "null volume pointer returns invalid pointer");

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0, "open valid port");
    Require(sceAudioOutSetVolume(port, 1, nullptr) == INVALID_POINTER, "null volume pointer on valid port returns invalid pointer");
    Require(sceAudioOutSetVolume(port, 1, vol) == 0, "valid port and volume returns success");
    Require(sceAudioOutClose(port) == 0, "close port");
    Require(sceAudioOutSetVolume(port, 1, vol) == INVALID_PORT, "closed port returns invalid port");

    std::puts("audio out set volume validation: PASS");
}
