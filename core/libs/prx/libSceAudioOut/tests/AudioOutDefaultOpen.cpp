#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SDL.h"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace {
SDL_AudioDeviceID device = 7;
const char* driver = "wasapi";
int outputDevices = 0;
bool endpointAbsent = false;
bool unplugged = false;
int probes = 0;
int opened = 0;
int closed = 0;
int queuedLive = 0;
int queuedDead = 0;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

bool live(SDL_AudioDeviceID id) { return id == device && !unplugged; }
}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() {
    ++probes;
    return endpointAbsent;
}
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return outputDevices; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Main speakers"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec* spec) { *spec = {}; spec->channels = 2; return 0; }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return driver; }
extern "C" const char* SDLCALL SDL_GetError() { return "test error"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    Require(name == nullptr && capture == 0, "the main port did not open the default output");
    device = static_cast<SDL_AudioDeviceID>(7 + opened);
    ++opened;
    unplugged = false;
    *obtained = *desired;
    return device;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID id, int) { Require(id == device, "wrong device paused"); }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID id) { Require(id == device, "wrong queue queried"); return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID id, const void*, Uint32) {
    if (!unplugged) Require(id == device, "wrong queue written");
    if (live(id)) ++queuedLive;
    else ++queuedDead;
    return 0;
}
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID id) { Require(id == device, "wrong queue cleared"); }
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID id) { Require(id == device, "wrong device closed"); ++closed; }
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID id) { return live(id) ? SDL_AUDIO_PLAYING : SDL_AUDIO_STOPPED; }

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "invalid test arguments");
        const std::string_view mode = argv[1];
        bool expectOpen = true;
        if (mode == "zero_default") {
            driver = "alsa";
            outputDevices = 0;
        } else if (mode == "wasapi_zero_default") {
            driver = "wasapi";
            outputDevices = 0;
        } else if (mode == "no_endpoint") {
            driver = "wasapi";
            outputDevices = 0;
            endpointAbsent = true;
            expectOpen = false;
        } else if (mode == "positive") {
            driver = "wasapi";
            outputDevices = 2;
        } else if (mode == "unknown") {
            driver = "wasapi";
            outputDevices = -1;
        } else if (mode == "disconnect") {
            driver = "wasapi";
            outputDevices = 2;
        } else {
            Require(false, "invalid default open mode");
        }
        if (mode == "disconnect") {
            const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
            Require(port >= 1 && opened == 1, "the main port did not open the default output");
            unplugged = true;
            std::array<std::int16_t, 256 * 2> samples{};
            Require(sceAudioOutOutput(port, samples.data()) == 256, "output after the host removed the default output failed");
            Require(opened == 2 && closed == 1, "the removed default output was not replaced");
            Require(queuedLive == 1 && queuedDead == 0, "output was queued to the removed default output");
            Require(sceAudioOutClose(port) == 0 && closed == 2, "the replacement output was not closed with its port");
            std::puts("default output open tests passed");
            return 0;
        }
        const int handle = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
        Require(handle >= 1, "the main port did not open");
        Require(opened == (expectOpen ? 1 : 0), expectOpen ? "the default output was not opened" : "the default output was opened with no WASAPI endpoint");
        Require(sceAudioOutClose(handle) == 0, "the main port did not close");
        Require(closed == opened, "the default output was not closed with its port");
        Require(probes == (driver == std::string_view("wasapi") && outputDevices == 0 ? 1 : 0), "the default endpoint was probed outside an empty WASAPI list");
        probes = 0;
        Require(SdlAudioReportsNoOutputEndpoint(driver, outputDevices) == !expectOpen, "the no-endpoint predicate disagrees with the mode");
        Require(!SdlAudioReportsNoOutputEndpoint(nullptr, 0), "a missing driver name skipped the default open");
        std::puts("default output open tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
