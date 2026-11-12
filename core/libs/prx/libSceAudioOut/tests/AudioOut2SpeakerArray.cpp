#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template<typename TFunction>
static bool RejectsArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct VbapParams {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
    std::uint32_t reserved;
};

static bool Near(float value, float expected) {
    return std::fabs(value - expected) < 1e-5f;
}

int main() {
    const AudioOut2Position square[4] = {{1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}};
    alignas(16) unsigned char memory[512] = {};
    VbapParams params{square, 4, 0, memory, sizeof(memory), 0};
    AudioOut2SpeakerArrayHandle array = nullptr;
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(nullptr, &params, nullptr); }), "a null handle output was accepted");
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(&array, nullptr, nullptr); }), "null parameters were accepted");
    VbapParams none = params;
    none.numSpeakers = 0;
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(&array, &none, nullptr); }), "an array without speakers was accepted");
    VbapParams tooMany = params;
    tooMany.numSpeakers = 33;
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(&array, &tooMany, nullptr); }), "33 speakers were accepted");
    VbapParams small = params;
    small.memorySize = 16;
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(&array, &small, nullptr); }), "memory too small for the array was accepted");
    VbapParams misaligned = params;
    misaligned.memory = memory + 1;
    Require(RejectsArgument([&] { sceAudioOut2SpeakerArrayCreate(&array, &misaligned, nullptr); }), "misaligned memory was accepted");
    Require(array == nullptr, "a rejected creation wrote the handle");
    Require(sceAudioOut2SpeakerArrayCreate(&array, &params, nullptr) == 0 && array == memory, "the array was not placed in the given memory");

    constexpr std::uint32_t ambisonics = 0x40;
    float coefficients[4] = {};
    Require(sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics | 0, coefficients, 4) == 0, "W channel coefficients failed");
    for (const float coefficient : coefficients) Require(Near(coefficient, 0.25f), "the W channel is not split evenly over four speakers");
    Require(sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics | 1, coefficients, 4) == 0, "Y channel coefficients failed");
    Require(Near(coefficients[0], 0.0f) && Near(coefficients[1], 0.25f) && Near(coefficients[2], 0.0f) && Near(coefficients[3], -0.25f), "the Y channel does not follow the sine of each azimuth");
    Require(sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics | 3, coefficients, 4) == 0, "X channel coefficients failed");
    Require(Near(coefficients[0], 0.25f) && Near(coefficients[1], 0.0f) && Near(coefficients[2], -0.25f) && Near(coefficients[3], 0.0f), "the X channel does not follow the cosine of each azimuth");
    Require(sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics | 4, coefficients, 4) == 0, "second-order coefficients failed");
    for (const float coefficient : coefficients) Require(coefficient == 0.0f, "four speakers decoded a second-order channel");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, 0, coefficients, 4); }), "a channel without the ambisonics flag was accepted");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics | 36, coefficients, 4); }), "a sixth-order channel was accepted");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics, coefficients, 3); }), "a coefficient count other than the speaker count was accepted");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(array, ambisonics, nullptr, 4); }), "null coefficients were accepted");
    alignas(16) unsigned char notArray[512] = {};
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(notArray, ambisonics, coefficients, 4); }), "memory that holds no array was accepted");

    float speakerCoefficients[4] = {3.0f, 3.0f, 3.0f, 3.0f};
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayCoefficients(array, square[0], 0.0f, nullptr, 4, 0, 0.0f); }),
        "null speaker-array coefficients were accepted");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayCoefficients(array, square[0], 0.0f, coefficients, 3, 0, 0.0f); }),
        "a coefficient count other than the speaker count was accepted for the speaker array");
    Require(RejectsArgument([&] { sceAudioOut2GetSpeakerArrayCoefficients(notArray, square[0], 0.0f, coefficients, 4, 0, 0.0f); }),
        "memory that holds no array was accepted for the speaker array");
    Require(sceAudioOut2GetSpeakerArrayCoefficients(array, square[0], 1.0f, speakerCoefficients, 4, 0, 0.0f) == 0,
        "the speaker-array coefficients failed");
    Require(Near(speakerCoefficients[0], 1.0f) && Near(speakerCoefficients[1], 1.0f), "the front pair is not at full gain");
    Require(Near(speakerCoefficients[2], 0.0f) && Near(speakerCoefficients[3], 0.0f), "a speaker beyond the front pair is not silent");
    const AudioOut2Position far{12345.0f, -6789.0f, 90.0f};
    Require(sceAudioOut2GetSpeakerArrayCoefficients(array, far, -2.5f, speakerCoefficients, 4, 1, 1e9f) == 0,
        "the speaker-array coefficients failed with extreme inputs");
    Require(Near(speakerCoefficients[0], 1.0f) && Near(speakerCoefficients[1], 1.0f) && Near(speakerCoefficients[2], 0.0f) &&
        Near(speakerCoefficients[3], 0.0f), "position, spread, height awareness or the downmix radius changed the coefficients");
    alignas(16) unsigned char monoMemory[512] = {};
    VbapParams mono = params;
    mono.numSpeakers = 1;
    mono.memory = monoMemory;
    AudioOut2SpeakerArrayHandle monoArray = nullptr;
    Require(sceAudioOut2SpeakerArrayCreate(&monoArray, &mono, nullptr) == 0 && monoArray == monoMemory, "a single-speaker array was not created");
    float firstCoefficient = 3.0f;
    Require(sceAudioOut2GetSpeakerArrayCoefficients(monoArray, square[0], 0.0f, &firstCoefficient, 1, 0, 0.0f) == 0,
        "the single-speaker coefficient failed");
    Require(Near(firstCoefficient, 1.0f), "the single speaker is not at full gain");

    std::puts("AudioOut2 speaker array tests passed");
    return 0;
}
