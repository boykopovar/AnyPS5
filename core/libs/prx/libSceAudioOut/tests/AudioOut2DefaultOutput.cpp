#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SDL.h"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
const char* defaultName = "Main speakers";
std::string openedName;
SDL_AudioDeviceID device = 0;
bool unplugged = false;
int opened = 0;
int closed = 0;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 2; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Main speakers"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec* spec) { *spec = {}; spec->channels = 2; return 0; }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return "wasapi"; }
extern "C" const char* SDLCALL SDL_GetError() { return "test error"; }
extern "C" int SDLCALL SDL_GetDefaultAudioInfo(char** name, SDL_AudioSpec* spec, int capture) {
    Require(capture == 0 && spec != nullptr, "the default output was not queried as an output");
    const auto bytes = std::strlen(defaultName) + 1;
    *name = static_cast<char*>(std::malloc(bytes));
    std::memcpy(*name, defaultName, bytes);
    return 0;
}
extern "C" void SDLCALL SDL_free(void* memory) { std::free(memory); }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    Require(capture == 0 && name != nullptr && std::string_view(name) == defaultName, "the main output was not opened on the default output by name");
    openedName = name;
    ++opened;
    device = static_cast<SDL_AudioDeviceID>(10 + opened);
    unplugged = false;
    *obtained = *desired;
    return device;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID id) {
    Require(id == device, "the wrong output was closed");
    ++closed;
}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID id) { return id == device && !unplugged ? SDL_AUDIO_PLAYING : SDL_AUDIO_STOPPED; }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32) { return 0; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) {}

int main() {
    try {
        Require(sceAudioOut2Initialize() == 0, "AudioOut2 did not initialize");
        AudioOut2ContextParam params{};
        Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters were not reset");
        params.queue_depth = 64;
        AudioOut2ContextHandle context = 0;
        Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0, "the context was not created");
        Require(opened == 1 && openedName == "Main speakers", "the context did not open the default output by name");
        defaultName = "Headset";
        Require(sceAudioOut2ContextPush(context, 0) == 0 && opened == 1, "the default output was checked again within a second");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        Require(sceAudioOut2ContextPush(context, 0) == 0, "a push after the host changed its default output failed");
        Require(opened == 2 && closed == 1 && openedName == "Headset", "the context did not follow the new default output");
        unplugged = true;
        Require(sceAudioOut2ContextPush(context, 0) == 0, "a push after the output went away failed");
        Require(opened == 3 && closed == 2, "the context did not reopen a stopped output");
        Require(sceAudioOut2ContextDestroy(context) == 0 && closed == 3, "the output was not closed with its context");
        std::puts("AudioOut2 default output tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
