#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "Ngs2Internal.hpp"

static const std::array<std::array<float, 4>, 512>& SamplerKernel() {
    static const auto table = [] {
        std::array<std::array<float, 4>, 512> result{};
        for (std::uint32_t phase = 0; phase < 512; ++phase) {
            std::array<double, 4> taps{};
            double sum = 0.0;
            for (std::uint32_t tap = 0; tap < 4; ++tap) {
                const double x = static_cast<double>(tap) - 1.0 - static_cast<double>(phase) / 512.0;
                const double sinc = x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
                taps[tap] = sinc * (0.5 + 0.5 * std::cos(std::numbers::pi * x / 2.0));
                sum += taps[tap];
            }
            for (std::uint32_t tap = 0; tap < 4; ++tap) result[phase][tap] = static_cast<float>(std::round(taps[tap] / sum * 1e10) / 1e10);
        }
        return result;
    }();
    return table;
}

void Ngs2RestartSampler(Ngs2Voice& voice) {
    voice.phase = 0;
    voice.samplerHistory = {};
    voice.samplerCachedFrames = 4;
    voice.samplerStarved = false;
    voice.samplerEnded = false;
    voice.pcmRemaining = 0;
}

void Ngs2SaveSamplerBlock(Ngs2Voice& voice, Ngs2Block& block) {
    block.submittedRepeats = block.info.num_repeats;
    if (voice.samplerWritePosition == voice.samplerReplayBlocks.size()) voice.samplerReplayBlocks.push_back(block);
    else voice.samplerReplayBlocks[voice.samplerWritePosition] = block;
    voice.samplerWritePosition = (voice.samplerWritePosition + 1) % std::max(1u, voice.rack->maxWaveformBlocks);
}

void Ngs2ReplaySampler(Ngs2Voice& voice) {
    const auto count = voice.samplerWritePosition != 0 ? voice.samplerWritePosition : voice.samplerReplayBlocks.size();
    voice.blocks.assign(voice.samplerReplayBlocks.begin(), voice.samplerReplayBlocks.begin() + count);
    for (auto& block : voice.blocks) {
        block.numRepeated = 0;
        block.dataCursor = 0;
        block.started = false;
        block.info.num_repeats = block.submittedRepeats;
    }
    ++voice.waveformRevision;
    voice.decodedSamples = 0;
    voice.decodedBytes = 0;
    voice.waveformData = nullptr;
    voice.waveformUserData = 0;
    if (voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) Ngs2RestartAtrac9(voice);
    Ngs2RestartSampler(voice);
}

void Ngs2PrepareSampler(Ngs2Voice& voice) {
    voice.samplerScratch.resize((static_cast<std::size_t>(voice.rack->maxGrainSamples) * 4 + 4) * voice.channels);
    Ngs2RestartSampler(voice);
    voice.pendingBlockCallbacks.reserve(voice.rack->maxWaveformBlocks);
    voice.dispatchBlockCallbacks.reserve(voice.rack->maxWaveformBlocks);
    SamplerKernel();
}

void Ngs2ConsumeSampler(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate) {
    const auto revision = voice.waveformRevision;
    const auto channels = voice.channels;
    const auto stride = grain * 4 + 4;
    if (voice.samplerScratch.size() < static_cast<std::size_t>(stride) * channels) {
        throw std::runtime_error("NGS2: sampler grain exceeds its prepared scratch buffer");
    }
    const float ratio = (1.0f / static_cast<float>(sampleRate)) * static_cast<float>(voice.sampleRate) * voice.pitch;
    const auto step = std::min(static_cast<std::uint32_t>(std::min(ratio, 4.0f) * 16384.0f), 65535u);
    const auto consumed = static_cast<std::uint32_t>((voice.phase + static_cast<std::uint64_t>(step) * grain) >> 14);
    const auto cached = voice.samplerCachedFrames;
    const auto requested = consumed + 4 - cached;
    for (std::uint32_t channel = 0; channel < channels; ++channel) {
        auto* dest = voice.samplerScratch.data() + static_cast<std::size_t>(channel) * stride;
        std::copy_n(voice.samplerHistory[channel].begin(), cached, dest);
        std::fill_n(dest + cached, requested, 0.0f);
    }
    const auto count = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9
        ? Ngs2ReadSamplerAtrac9(voice, voice.samplerScratch.data() + cached, stride, requested)
        : Ngs2ReadSamplerPcm(voice, voice.samplerScratch.data() + cached, stride, requested);
    if (voice.waveformRevision != revision) return;
    const auto& kernel = SamplerKernel();
    for (std::uint32_t channel = 0; channel < channels; ++channel) {
        const auto* source = voice.samplerScratch.data() + static_cast<std::size_t>(channel) * stride;
        auto* dest = voice.samples.data() + static_cast<std::size_t>(channel) * grain;
        if (step == 16384 && voice.phase == 0) {
            std::copy_n(source + 1, grain, dest);
        } else {
            auto phase = static_cast<std::uint32_t>(voice.phase);
            for (std::uint32_t i = 0; i < grain; ++i) {
                const auto index = phase >> 14;
                const auto& coefficients = kernel[(phase & 0x3fff) >> 5];
                const float a = source[index] * coefficients[0];
                const float b = source[index + 1] * coefficients[1];
                const float c = source[index + 2] * coefficients[2];
                const float d = source[index + 3] * coefficients[3];
                dest[i] = (a + b) + (c + d);
                phase += step;
            }
        }
        std::copy_n(source + consumed, 4, voice.samplerHistory[channel].begin());
    }
    voice.phase = (voice.phase + static_cast<std::uint64_t>(step) * grain) & 0x3fff;
    voice.samplerCachedFrames = cached + count > consumed ? std::min(4u, cached + count - consumed) : 0;
    voice.samplerStarved = count < requested;
    voice.samplerEnded = voice.samplerStarved && !voice.acceptsBlocks;
    voice.hasSamples = count != 0 || std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
}
