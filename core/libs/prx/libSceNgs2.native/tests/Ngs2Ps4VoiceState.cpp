#include "Ngs2Test.hpp"
#include "prx/libSceNgs2.native/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <vector>

static bool Untouched(const std::array<unsigned char, 256>& buffer) {
    return std::all_of(buffer.begin(), buffer.end(), [](auto byte) { return byte == 0xaa; });
}

static bool NearEqual(float a, float b, float tolerance = 1e-3f) {
    return std::fabs(a - b) <= tolerance;
}

static void FftInitAndProcess() {
    std::uint64_t realSize = 0;
    Require(sceNgs2FftQuerySize(8, 2, &realSize) == 0 && realSize == 1088u);

    std::vector<std::uint8_t> work(realSize, 0xa5);
    void* handle = reinterpret_cast<void*>(std::uintptr_t{0xa5a5a5a5});
    Require(sceNgs2FftInit(work.data(), realSize - 1, 8, 2, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), realSize, 7, 2, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), realSize, 13, 2, &handle) == -1);
    Require(sceNgs2FftInit(nullptr, realSize, 8, 2, &handle) == -1);
    Require(sceNgs2FftInit(work.data(), realSize, 8, 2, nullptr) == -1);
    Require(handle == reinterpret_cast<void*>(std::uintptr_t{0xa5a5a5a5}));

    Require(sceNgs2FftInit(work.data(), realSize, 8, 2, &handle) == 0);
    Require(handle == work.data());

    std::uint64_t tableAddr = 0;
    std::uint64_t secondAddr = 0;
    std::int32_t storedLen = 0;
    std::uint32_t storedFlags = 0;
    std::memcpy(&tableAddr, work.data() + 0x00, sizeof(tableAddr));
    std::memcpy(&secondAddr, work.data() + 0x08, sizeof(secondAddr));
    std::memcpy(&storedLen, work.data() + 0x10, sizeof(storedLen));
    std::memcpy(&storedFlags, work.data() + 0x14, sizeof(storedFlags));
    const std::uint64_t expectedTable = (reinterpret_cast<std::uintptr_t>(work.data()) + 0x57u) & ~std::uintptr_t{0x3fu};
    Require(tableAddr == expectedTable);
    Require(secondAddr == expectedTable + 32u * 24u);
    Require(storedLen == 8 && storedFlags == 2u);

    constexpr std::size_t points = 256;
    std::vector<float> impulse(points, 0.0f);
    impulse[0] = 1.0f;
    std::vector<float> scratch(points, 0.0f);
    std::vector<std::uint8_t> outBytes((points + 16) * sizeof(float), 0xa5);
    auto* outPtr = reinterpret_cast<float*>(outBytes.data());

    Require(sceNgs2FftProcess(nullptr, impulse.data(), outPtr, scratch.data(), 0, 1) == -1);
    Require(sceNgs2FftProcess(handle, nullptr, outPtr, scratch.data(), 0, 1) == -1);
    Require(sceNgs2FftProcess(handle, impulse.data(), nullptr, scratch.data(), 0, 1) == -1);

    Require(sceNgs2FftProcess(handle, impulse.data(), outPtr, scratch.data(), 0, 1) == 0);
    std::vector<float> out(points);
    std::memcpy(out.data(), outBytes.data(), points * sizeof(float));
    Require(NearEqual(out[0], 1.0f) && NearEqual(out[1], 1.0f));
    for (std::size_t k = 1; k < points / 2; ++k) {
        Require(NearEqual(out[k * 2], 1.0f));
        Require(NearEqual(out[k * 2 + 1], 0.0f));
    }
    for (std::size_t i = points * sizeof(float); i < outBytes.size(); ++i) {
        Require(outBytes[i] == 0xa5);
    }

    constexpr double twoPi = 6.28318530717958647692;
    std::vector<float> signal(points, 0.0f);
    for (std::size_t n = 0; n < points; ++n) {
        const double angle3 = (twoPi * 3.0 * static_cast<double>(n)) / static_cast<double>(points);
        const double angle7 = (twoPi * 7.0 * static_cast<double>(n)) / static_cast<double>(points);
        const float alt = (n & 1u) != 0 ? -0.25f : 0.25f;
        signal[n] = 0.5f + static_cast<float>(std::cos(angle3)) - 0.5f * static_cast<float>(std::sin(angle7)) + alt;
    }

    outBytes.assign((points + 16) * sizeof(float), 0xa5);
    Require(sceNgs2FftProcess(handle, signal.data(), outPtr, nullptr, 0, 1u | 8u) == 0);
    std::memcpy(out.data(), outBytes.data(), points * sizeof(float));
    Require(NearEqual(out[0], 0.5f) && NearEqual(out[1], 0.25f));
    Require(NearEqual(out[6], 0.5f) && NearEqual(out[7], 0.0f));
    Require(NearEqual(out[14], 0.0f) && NearEqual(out[15], 0.25f));
    for (std::size_t i = points * sizeof(float); i < outBytes.size(); ++i) {
        Require(outBytes[i] == 0xa5);
    }

    std::vector<float> unscaledSpectrum(points, 0.0f);
    Require(sceNgs2FftProcess(handle, signal.data(), unscaledSpectrum.data(), scratch.data(), 0, 1u) == 0);
    std::vector<float> reconstructed(points, 0.0f);
    Require(sceNgs2FftProcess(handle, unscaledSpectrum.data(), reconstructed.data(), scratch.data(), 0, 8u) == 0);
    for (std::size_t n = 0; n < points; ++n) {
        Require(NearEqual(reconstructed[n], signal[n], 1e-2f));
    }

    std::vector<float> internalSpectrum(points, 0.0f);
    std::vector<float> internalReconstructed(points, 0.0f);
    Require(sceNgs2FftProcess(handle, signal.data(), internalSpectrum.data(), scratch.data(), 0, 1u | 4u) == 0);
    Require(sceNgs2FftProcess(handle, internalSpectrum.data(), internalReconstructed.data(), scratch.data(), 0, 4u | 8u) == 0);
    for (std::size_t n = 0; n < points; ++n) {
        Require(NearEqual(internalReconstructed[n], signal[n], 1e-2f));
    }
}

static void InvalidVoice(uintptr_t voice) {
    std::uint32_t flags = 0xaaaaaaaa;
    Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_ERROR_PS4_INVALID_VOICE_HANDLE && flags == 0);
    for (const std::size_t size : {std::size_t{64}, std::size_t{4}, std::size_t{0}, std::size_t{3}}) {
        alignas(16) std::array<unsigned char, 256> buffer;
        buffer.fill(0xaa);
        const int expected = size < 4 ? SCE_NGS2_ERROR_PS4_INVALID_STATE_SIZE : SCE_NGS2_ERROR_PS4_INVALID_VOICE_HANDLE;
        Require(sceNgs2VoiceGetState(voice, reinterpret_cast<Ngs2VoiceState*>(buffer.data()), size) == expected);
        Require(Untouched(buffer));
    }
}

int main() {
    try {
        FftInitAndProcess();
        alignas(16) static std::array<unsigned char, 256> zeroedVoice{};
        InvalidVoice(reinterpret_cast<uintptr_t>(zeroedVoice.data()));
        InvalidVoice(0x1234);
        const auto system = CreateSystem();
        const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
        const auto voice = Voice(rack);
        std::uint32_t flags = 0xaaaaaaaa;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK && flags == 0);
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        InvalidVoice(voice);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        std::cout << "NGS2 PS4 voice state errors checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
