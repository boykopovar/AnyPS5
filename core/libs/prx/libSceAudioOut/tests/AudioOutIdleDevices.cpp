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
struct FakeDevice {
    bool open = false;
    bool paused = true;
    bool stopped = false;
    int cleared = 0;
    int queued = 0;
};

std::array<FakeDevice, 16> devices{};
const char* driver = "wasapi";
int opened = 0;
int closed = 0;
SDL_AudioDeviceID lastClosed = 0;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

FakeDevice& Device(SDL_AudioDeviceID id) {
    Require(id >= 1 && id <= devices.size() && devices[id - 1].open, "an SDL call used a device that is not open");
    return devices[id - 1];
}

int Open(std::uint32_t frames) {
    const int handle = sceAudioOutOpen(0, 0, 0, frames, 48000, 1);
    Require(handle > 0, "a main port did not open");
    return handle;
}

void Close(int handle) {
    Require(sceAudioOutClose(handle) == 0, "a main port did not close");
}

void Output(int handle, std::uint32_t frames) {
    std::array<std::int16_t, 1280 * 2> samples{};
    Require(sceAudioOutOutput(handle, samples.data()) == static_cast<int>(frames), "a main port output failed");
}

void RunReuse() {
    const int first = Open(256);
    Require(opened == 1 && !devices[0].paused, "the first port did not open and start a device");
    Output(first, 256);
    Require(devices[0].queued == 1, "audio did not reach the opened device");
    const int clearedBeforeClose = devices[0].cleared;
    Close(first);
    Require(closed == 0 && devices[0].open && devices[0].paused && devices[0].cleared > clearedBeforeClose, "a closed port did not park its paused, flushed device");
    const int clearedBeforeReuse = devices[0].cleared;
    const int second = Open(256);
    Require(opened == 1 && !devices[0].paused && devices[0].cleared > clearedBeforeReuse, "an open with the same request did not resume the flushed parked device");
    Output(second, 256);
    Require(devices[0].queued == 2, "audio after reuse did not reach the parked device");
    Close(second);
    const int other = Open(512);
    Require(opened == 2 && devices[0].paused && !devices[1].paused, "an open with another request took the parked device");
    Close(other);
    Require(closed == 0 && devices[0].open && devices[1].open, "parking two devices with different requests closed one");
}

void RunLimit() {
    constexpr std::array<std::uint32_t, 5> frames{256, 512, 768, 1024, 1280};
    std::array<int, 5> handles{};
    for (std::size_t i = 0; i < frames.size(); ++i) handles[i] = Open(frames[i]);
    Require(opened == 5, "five ports with different requests did not open five devices");
    for (const int handle : handles) Close(handle);
    Require(closed == 1 && lastClosed == 1 && !devices[0].open, "a fifth parked device did not displace the oldest");
    for (std::size_t i = 1; i < frames.size(); ++i) Require(devices[i].open && devices[i].paused, "a device within the limit was not kept parked");
    const int displaced = Open(frames[0]);
    Require(opened == 6, "the request of a displaced device did not open a new device");
    Close(displaced);
    Require(closed == 2 && lastClosed == 2 && !devices[1].open && devices[5].open, "parking over the limit did not displace the oldest parked device");
    const int kept = Open(frames[4]);
    Require(opened == 6 && !devices[4].paused, "a device still parked was not reused");
    Close(kept);
}

void RunStopped() {
    const int first = Open(256);
    Close(first);
    devices[0].stopped = true;
    const int second = Open(256);
    Require(closed == 1 && lastClosed == 1 && opened == 2 && !devices[1].paused, "a parked device that stopped was reused instead of closed and replaced");
    devices[1].stopped = true;
    Close(second);
    Require(closed == 2 && lastClosed == 2, "a stopped device was parked instead of closed with its port");
    const int third = Open(256);
    Require(opened == 3, "an open after a stopped device closed did not open a new device");
    Close(third);
}

void RunDisk() {
    driver = "disk";
    const int first = Open(256);
    Close(first);
    Require(opened == 1 && closed == 1 && lastClosed == 1, "the disk driver parked a device instead of closing it with its port");
    const int second = Open(256);
    Require(opened == 2, "the disk driver reused a closed device");
    Close(second);
    Require(closed == 2, "the disk driver did not close the second device with its port");
}
}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Main speakers"; }
extern "C" int SDLCALL SDL_GetAudioDeviceSpec(int, int, SDL_AudioSpec* spec) { *spec = {}; spec->channels = 2; return 0; }
extern "C" const char* SDLCALL SDL_GetCurrentAudioDriver() { return driver; }
extern "C" const char* SDLCALL SDL_GetError() { return "test error"; }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    Require(name == nullptr && capture == 0, "the main port did not open the default output");
    Require(opened < static_cast<int>(devices.size()), "too many devices opened");
    ++opened;
    devices[opened - 1] = FakeDevice{};
    devices[opened - 1].open = true;
    *obtained = *desired;
    return static_cast<SDL_AudioDeviceID>(opened);
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID id, int pauseOn) { Device(id).paused = pauseOn != 0; }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID id) { Device(id); return 0; }
extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID id, const void*, Uint32) {
    auto& device = Device(id);
    Require(!device.paused, "audio was queued to a paused device");
    ++device.queued;
    return 0;
}
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID id) { ++Device(id).cleared; }
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID id) {
    Device(id).open = false;
    ++closed;
    lastClosed = id;
}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID id) {
    const auto& device = Device(id);
    if (device.stopped) return SDL_AUDIO_STOPPED;
    return device.paused ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "invalid test arguments");
        const std::string_view mode = argv[1];
        if (mode == "reuse") RunReuse();
        else if (mode == "limit") RunLimit();
        else if (mode == "stopped") RunStopped();
        else if (mode == "disk") RunDisk();
        else Require(false, "invalid idle device mode");
        std::puts("audio out idle device tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
