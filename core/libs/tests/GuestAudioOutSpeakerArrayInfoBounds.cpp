#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

struct GuestVbapParams {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
    std::uint32_t reserved;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSpeakerArrayMemorySize() {
    const std::size_t size1 = sceAudioOut2GetSpeakerArrayMemorySize(1, 0, 0);
    Require(size1 >= sizeof(AudioOut2Position));

    const std::size_t size2 = sceAudioOut2GetSpeakerArrayMemorySize(2, 1, 1);
    Require(size2 == size1);

    const std::size_t size32 = sceAudioOut2GetSpeakerArrayMemorySize(32, 1, 0);
    Require(size32 == size1);
}

void TestSpeakerArrayDestroyedHandle() {
    alignas(16) std::array<std::uint8_t, 512> memory{};
    std::array<AudioOut2Position, 2> positions = {{{1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}}};
    AudioOut2SpeakerArrayHandle handle = nullptr;

    GuestVbapParams params{positions.data(), 2, 1, memory.data(), memory.size(), 0};
    Require(sceAudioOut2SpeakerArrayCreate(&handle, &params, nullptr) == 0);
    Require(handle != nullptr);

    Require(sceAudioOut2SpeakerArrayDestroy(handle) == 0);

    bool threw = false;
    try {
        std::array<float, 2> coeffs{};
        sceAudioOut2GetSpeakerArrayCoefficients(handle, AudioOut2Position{}, 0.0f, coeffs.data(), 2, 0, 0.0f);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        std::array<float, 2> coeffs{};
        sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0x40, coeffs.data(), 2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOut2SpeakerArrayDestroy(handle);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestSpeakerArrayMemorySize();
    TestSpeakerArrayDestroyedHandle();

    LibcRunShutdown_nid_postfix();
    return 0;
}
