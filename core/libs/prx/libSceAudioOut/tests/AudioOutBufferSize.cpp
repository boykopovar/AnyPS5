#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SDL.h"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr SDL_AudioDeviceID device = 7;
int opened = 0;
int closed = 0;
int queued = 0;
std::uint32_t queuedBytes = 0;
const void* supplied = nullptr;
std::uint32_t suppliedBytes = 0;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void RunPassThrough() {
    std::vector<float> samples(512u * 8u);
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<float>(i % 97u) / 97.0f;
    const int handle = sceAudioOutOpen(0, 0, 0, 512, 48000, 5);
    Require(handle > 0 && opened == 1, "a 512-frame F32 8-channel port did not open its device");
    supplied = samples.data();
    suppliedBytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
    Require(sceAudioOutOutput(handle, samples.data()) == 512, "a 512-frame output did not return 512");
    Require(queued == 1 && queuedBytes == 16384, "a 512-frame F32 8-channel output did not queue 16384 bytes");
    Require(sceAudioOutClose(handle) == 0 && closed == 1, "the port did not close its device");
}

void RunOverflow(std::uint32_t frames, std::uint32_t format, std::uint32_t frameBytes) {
    std::vector<std::uint8_t> frame(frameBytes);
    const int handle = sceAudioOutOpen(0, 0, 0, frames, 48000, format);
    Require(handle > 0 && opened == 1, "a port with an oversized length did not open its device");
    supplied = frame.data();
    suppliedBytes = frameBytes;
    bool threw = false;
    try {
        static_cast<void>(sceAudioOutOutput(handle, frame.data()));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "an output whose buffer size overflows did not throw");
    Require(queued == 0, "an output whose buffer size overflows queued audio");
    Require(sceAudioOutClose(handle) == 0 && closed == 1, "the port did not close after the refused output");
}
}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Main speakers"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec* spec) { *spec = {}; spec->channels = 2; return 0; }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return "dummy"; }
extern "C" const char* SDLCALL SDL_GetError() { return "test error"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    Require(name == nullptr && capture == 0 && desired->callback == nullptr, "the main port did not open the default device");
    ++opened;
    *obtained = *desired;
    return device;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID id, int) { Require(id == device, "wrong device paused"); }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID id) { Require(id == device, "wrong queue queried"); return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID id, const void* data, Uint32 bytes) {
    Require(id == device, "wrong queue fed");
    ++queued;
    queuedBytes = bytes;
    if (bytes <= suppliedBytes) Require(std::memcmp(data, supplied, bytes) == 0, "the queued audio is not the supplied samples");
    return 0;
}
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID id) { Require(id == device, "wrong queue cleared"); }
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID id) { Require(id == device, "wrong device closed"); ++closed; }

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "invalid test arguments");
        const std::string_view mode = argv[1];
        if (mode == "pass_through") RunPassThrough();
        else if (mode == "wrap") RunOverflow(0x80000000u, 5, 8u * sizeof(float));
        else if (mode == "mix") RunOverflow(0x80000000u, 7, 8u * sizeof(float));
        else if (mode == "threshold") RunOverflow(0x40000000u, 0, sizeof(std::int16_t));
        else Require(false, "invalid buffer size mode");
        std::puts("audio out buffer size tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
