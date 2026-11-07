#include <bit>
#include <cstdint>
#include <stdexcept>

#include "Ngs2Internal.hpp"

std::size_t Ngs2PcmSampleBytes(std::uint32_t waveformType) {
    switch (waveformType) {
        case SCE_NGS2_WAVEFORM_TYPE_PCM_U8: return 1;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I16L: return 2;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I24L: return 3;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I32L:
        case SCE_NGS2_WAVEFORM_TYPE_PCM_F32L: return 4;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_F64L: return 8;
        default: throw std::runtime_error("NGS2: PCM waveform type " + Ngs2Hex(waveformType) + " is not implemented");
    }
}

static std::uint64_t ReadLittleEndian(const std::uint8_t* data, std::size_t bytes) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; i++) value |= static_cast<std::uint64_t>(data[i]) << (i * 8);
    return value;
}

static void CheckFloatRange(std::uint64_t magnitude, std::uint64_t minimum, std::uint64_t maximum) {
    if (magnitude > maximum || (magnitude != 0 && magnitude < minimum)) {
        throw std::runtime_error("NGS2: nonfinite, subnormal or out-of-range floating-point PCM is not implemented");
    }
}

static float ReadSample(std::uint32_t waveformType, const std::uint8_t* sample, std::size_t bytes) {
    const auto value = ReadLittleEndian(sample, bytes);
    switch (waveformType) {
        case SCE_NGS2_WAVEFORM_TYPE_PCM_U8:
            return (static_cast<int>(value) - 128) / 128.0f;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I16L:
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value)) / 32768.0f;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I24L:
            return (static_cast<std::int32_t>(value ^ 0x800000) - 0x800000) / 8388608.0f;
        case SCE_NGS2_WAVEFORM_TYPE_PCM_I32L:
            return static_cast<float>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)) / 2147483648.0);
        case SCE_NGS2_WAVEFORM_TYPE_PCM_F32L:
            CheckFloatRange(value & 0x7fffffff, 0x00800000, 0x3f800000);
            return std::bit_cast<float>(static_cast<std::uint32_t>(value));
        case SCE_NGS2_WAVEFORM_TYPE_PCM_F64L:
            CheckFloatRange(value & 0x7fffffffffffffff, 0x3810000000000000, 0x3ff0000000000000);
            return static_cast<float>(std::bit_cast<double>(value));
        default: throw std::runtime_error("NGS2: PCM sample conversion is not implemented");
    }
}

void Ngs2ReadPcmFrame(std::uint32_t waveformType, std::uint32_t channels, const std::uint8_t* frame, float* output) {
    const auto bytes = Ngs2PcmSampleBytes(waveformType);
    for (std::uint32_t channel = 0; channel < channels; channel++) {
        output[channel] = frame == nullptr ? 0.0f : ReadSample(waveformType, frame + channel * bytes, bytes);
    }
}
