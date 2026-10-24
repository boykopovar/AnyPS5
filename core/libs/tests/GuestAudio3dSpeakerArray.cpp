#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

struct Audio3dSpeakerArrayParameters {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
};

extern "C" {
size_t APS5_VABI sceAudio3dGetSpeakerArrayMemorySize(std::uint32_t num_speakers, std::uint8_t is_3d);
int APS5_VABI sceAudio3dCreateSpeakerArray(AudioOut2SpeakerArrayHandle* handle, const Audio3dSpeakerArrayParameters* parameters);
int APS5_VABI sceAudio3dDeleteSpeakerArray(AudioOut2SpeakerArrayHandle handle);
int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, std::uint32_t num_coefficients);
int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients2(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, std::uint32_t num_coefficients, std::uint8_t height_aware, float downmix_spread_radius);
size_t APS5_VABI sceAudioOut2GetSpeakerArrayMemorySize(std::uint32_t num_speakers, std::uint8_t is_3d, std::uint8_t is_ambisonics);
}

namespace {

constexpr int INVALID_PARAMETER = static_cast<int>(0x80EA0004);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename F>
void RequireThrows(F f, const char* message) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, message);
}

}

int main() {
    Require(sceAudio3dGetSpeakerArrayMemorySize(4, 1) == sceAudioOut2GetSpeakerArrayMemorySize(4, 1, 0), "memory size forwards to AudioOut2 without ambisonics");
    RequireThrows([] { sceAudio3dGetSpeakerArrayMemorySize(0, 0); }, "zero speakers throws invalid_argument");

    const AudioOut2Position positions[4] = {{1, 1, 0}, {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0}};
    alignas(16) unsigned char memory[1024] = {};
    const std::size_t size = sceAudio3dGetSpeakerArrayMemorySize(4, 0);
    Require(size <= sizeof(memory), "speaker array fits the test buffer");
    const Audio3dSpeakerArrayParameters parameters{positions, 4, 0, memory, size};

    AudioOut2SpeakerArrayHandle handle = nullptr;
    Require(sceAudio3dCreateSpeakerArray(nullptr, &parameters) == INVALID_PARAMETER, "null handle is an invalid parameter");
    Require(sceAudio3dCreateSpeakerArray(&handle, nullptr) == INVALID_PARAMETER, "null parameters are an invalid parameter");
    Require(handle == nullptr, "a rejected create leaves the handle");
    Require(sceAudio3dCreateSpeakerArray(&handle, &parameters) == 0, "create succeeds without ambisonics parameters");
    Require(handle != nullptr, "create stores the handle");

    float coefficients[4] = {-1, -1, -1, -1};
    Require(sceAudio3dGetSpeakerArrayMixCoefficients(handle, AudioOut2Position{1, 0, 0}, 0.0f, coefficients, 4) == 0, "mix coefficients succeed");
    Require(coefficients[2] == 0.0f && coefficients[3] == 0.0f, "mix coefficients are written");
    float coefficients2[4] = {-1, -1, -1, -1};
    Require(sceAudio3dGetSpeakerArrayMixCoefficients2(handle, AudioOut2Position{1, 0, 0}, 0.0f, coefficients2, 4, 1, 0.5f) == 0, "mix coefficients 2 succeed");
    Require(coefficients2[3] == 0.0f, "mix coefficients 2 are written");
    RequireThrows([&] { sceAudio3dGetSpeakerArrayMixCoefficients(handle, AudioOut2Position{1, 0, 0}, 0.0f, coefficients, 3); }, "a coefficient count that differs from the array is rejected");
    RequireThrows([&] { sceAudio3dGetSpeakerArrayMixCoefficients(handle, AudioOut2Position{1, 0, 0}, 0.0f, nullptr, 4); }, "null coefficients pointer throws invalid_argument");

    Require(sceAudio3dDeleteSpeakerArray(handle) == 0, "delete succeeds");
    RequireThrows([&] { sceAudio3dDeleteSpeakerArray(handle); }, "a deleted array is no longer valid");
    return 0;
}
