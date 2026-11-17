#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "SDL.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
const char* defaultName = "Headset microphone";
std::string openedName;
SDL_AudioDeviceID device = 0;
bool unplugged = false;
int opened = 0;
int closed = 0;

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

extern "C" std::uint64_t HostAudioInMuteState_nid_no_patch() { return 0; }
extern "C" int SDLCALL SDL_InitSubSystem(Uint32) { return 0; }
extern "C" int SDLCALL SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDLCALL SDL_GetAudioDeviceName(int, int) { return "Headset microphone"; }
extern "C" int SDLCALL SDL_GetDefaultAudioInfo(char** name, SDL_AudioSpec* spec, int capture) {
    Require(capture == 1 && spec != nullptr, "the default input was not queried as a capture device");
    const auto bytes = std::strlen(defaultName) + 1;
    *name = static_cast<char*>(std::malloc(bytes));
    std::memcpy(*name, defaultName, bytes);
    return 0;
}
extern "C" void SDLCALL SDL_free(void* memory) { std::free(memory); }
extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char* name, int capture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    Require(capture == 1 && name != nullptr && std::string_view(name) == defaultName, "the port was not opened on the default input by name");
    openedName = name;
    ++opened;
    device = static_cast<SDL_AudioDeviceID>(20 + opened);
    unplugged = false;
    *obtained = *desired;
    return device;
}
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID id) {
    Require(id == device, "the wrong input was closed");
    ++closed;
}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID id) { return id == device && !unplugged ? SDL_AUDIO_PLAYING : SDL_AUDIO_STOPPED; }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { return 0; }
extern "C" Uint32 SDLCALL SDL_DequeueAudio(SDL_AudioDeviceID, void*, Uint32) { return 0; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) {}

int main() {
    try {
        std::array<std::int16_t, 256 * 2> block{};
        const int port = sceAudioInOpen(1, 0, 0, 256, 48000, 2);
        Require(port > 0 && opened == 1 && openedName == "Headset microphone", "the port did not open the default input by name");
        defaultName = "Webcam microphone";
        sceAudioInInput(port, block.data());
        Require(opened == 1, "the default input was checked again within a second");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        sceAudioInInput(port, block.data());
        Require(opened == 2 && closed == 1 && openedName == "Webcam microphone", "the port did not follow the new default input");
        unplugged = true;
        sceAudioInInput(port, block.data());
        Require(opened == 3 && closed == 2, "the port did not reopen a stopped input");
        Require(sceAudioInClose(port) == 0 && closed == 3, "the input was not closed with its port");
        std::puts("AudioIn default input tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
