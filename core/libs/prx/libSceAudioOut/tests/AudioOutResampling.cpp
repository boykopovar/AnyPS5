#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SDL.h"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr std::uint32_t grain = 256;
constexpr int hostRate = 48000;
SDL_AudioStream* conversion = nullptr;
std::vector<float> played;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Collect() {
    const int bytes = SDL_AudioStreamAvailable(conversion);
    Require(bytes >= 0 && bytes % static_cast<int>(sizeof(float)) == 0, "invalid converted size");
    const auto first = played.size();
    played.resize(first + bytes / sizeof(float));
    if (bytes != 0) Require(SDL_AudioStreamGet(conversion, played.data() + first, bytes) == bytes, "stream read failed");
}

void TestBoundary(std::uint32_t sourceRate) {
    played.clear();
    const int port = sceAudioOutOpen(0, 0, 0, grain, sourceRate, 4);
    Require(port > 0, "port creation failed");
    std::vector<float> data(grain * 2, 0.0f);
    data[(grain - 1) * 2] = 0.5f;
    data[(grain - 1) * 2 + 1] = 0.25f;
    Require(sceAudioOutOutput(port, data.data()) == static_cast<int>(grain), "first output failed");
    data.assign(grain * 2, 0.0f);
    for (int block = 0; block < 8; block++) Require(sceAudioOutOutput(port, data.data()) == static_cast<int>(grain), "trailing output failed");
    Require(sceAudioOutOutput(port, nullptr) == static_cast<int>(grain), "drain failed");
    Require(sceAudioOutClose(port) == 0, "close failed");
    const auto boundary = static_cast<std::size_t>(std::ceil(static_cast<double>(grain) * hostRate / sourceRate));
    Require(played.size() / 2 > boundary + 16, "resampled tail missing");
    double tailEnergy = 0.0;
    for (std::size_t frame = boundary; frame < boundary + 16; frame++) {
        const float left = played[frame * 2];
        const float right = played[frame * 2 + 1];
        Require(std::isfinite(left) && std::isfinite(right), "nonfinite resampled output");
        Require(std::abs(left - 2.0f * right) < 1e-6f, "stereo channels changed during conversion");
        tailEnergy += static_cast<double>(left) * left;
    }
    Require(tailEnergy > 1e-7, "resampling discarded the impulse response at the grain boundary");
}

}

extern "C" int SDLCALL SDL_InitSubSystem(Uint32) { return 0; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID) { return SDL_AUDIO_PLAYING; }
extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { return 0; }
extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID) {}

extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int changes) {
    *obtained = *desired;
    if ((changes & SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) != 0) obtained->freq = hostRate;
    else {
        conversion = SDL_NewAudioStream(desired->format, desired->channels, desired->freq, AUDIO_F32SYS, 2, hostRate);
        Require(conversion != nullptr, "host conversion stream creation failed");
    }
    return 2;
}

extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID, const void* data, Uint32 size) {
    if (conversion != nullptr) {
        if (SDL_AudioStreamPut(conversion, data, static_cast<int>(size)) < 0) return -1;
        Collect();
    } else {
        const auto* samples = static_cast<const float*>(data);
        played.insert(played.end(), samples, samples + size / sizeof(float));
    }
    return 0;
}

extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID) {
    if (conversion == nullptr) return;
    Require(SDL_AudioStreamFlush(conversion) == 0, "host conversion flush failed");
    Collect();
    SDL_FreeAudioStream(conversion);
    conversion = nullptr;
}

int main() {
    TestBoundary(32000);
    TestBoundary(44100);
    return 0;
}
