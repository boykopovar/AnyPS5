#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "PitchShift.hpp"
#include "Ngs2Internal.hpp"

static constexpr float PI = std::numbers::pi_v<float>;
static constexpr float TWO_PI = 2.0f * PI;

static float TableSin(float angle) {
    if (angle < 0.0f || angle > TWO_PI) angle -= std::floor(angle / TWO_PI) * TWO_PI;
    const float phase = (angle / TWO_PI) * 256.0f;
    const auto index = static_cast<std::uint32_t>(phase);
    const float fraction = phase - static_cast<float>(index);
    const auto sample = [](std::uint32_t i) {
        if ((i & 127u) == 0) return 0.0f;
        return static_cast<float>(std::sin(static_cast<double>(i & 255u) * (2.0 * std::numbers::pi / 256.0)));
    };
    return (1.0f - fraction) * sample(index) + fraction * sample(index + 1);
}

void Ngs2PitchShiftChannel::SetPitch(float ratio) {
    coefficients.fill(0.0f);
    const float rate = ratio * 48000.0f;
    if (rate <= 48000.0f) {
        coefficients[7] = 1.0f;
        return;
    }
    const float cutoff = 15840.0009765625f / rate;
    for (std::uint32_t i = 0; i < coefficients.size(); ++i) {
        const float windowAngle = (static_cast<float>(i) * TWO_PI) / 14.0f;
        const float window = -0.46f * TableSin(windowAngle + PI / 2.0f) + 0.54f;
        const auto tap = static_cast<std::int32_t>(i) - 7;
        const float sinc = tap == 0 ? 2.0f * cutoff : TableSin(static_cast<float>(tap) * (cutoff * TWO_PI)) / (static_cast<float>(tap) * PI);
        coefficients[14 - i] = sinc * window;
    }
}

static float FilterSample(const Ngs2PitchShiftChannel& channel, std::uint32_t index) {
    float result = 0.0f;
    std::uint32_t tap = 0;
    while (tap < 15) {
        const auto count = std::min(15u - tap, 142u - index);
        std::array<float, 4> lanes{};
        std::uint32_t j = 0;
        for (; j + 4 <= count; j += 4) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) lanes[lane] += channel.coefficients[tap + j + lane] * channel.filterHistory[index + j + lane];
        }
        result += ((lanes[0] + lanes[1]) + lanes[2]) + lanes[3];
        for (; j < count; ++j) result += channel.coefficients[tap + j] * channel.filterHistory[index + j];
        tap += count;
        index = (index + count) % 142;
    }
    return result;
}

struct PitchShiftStep {
    std::uint32_t consumed;
    std::uint32_t produced;
    std::uint32_t inputTime;
    std::uint32_t outputTime;
};

static std::uint32_t FindOverlap(const float* reference, const float* candidate, std::uint32_t first, std::uint32_t last) {
    float best = 2147483648.0f;
    std::uint32_t selected = 0;
    for (std::uint32_t offset = first; offset <= last; offset += 8) {
        std::array<float, 4> lanes{};
        for (std::uint32_t block = 0; block < 8; ++block) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                lanes[lane] += std::abs(reference[block * 16 + lane] - candidate[offset + block * 16 + lane]);
            }
        }
        const float difference = ((lanes[0] + lanes[1]) + lanes[2]) + lanes[3];
        if (best > difference) {
            best = difference + 0.0f;
            selected = offset;
        }
    }
    return selected;
}

static void Crossfade(const float* from, const float* to, float* dest, std::uint32_t count) {
    const float step = 1.0f / static_cast<float>(count);
    std::array<float, 4> fade{0.0f, step, step + step, step * 3.0f};
    for (std::uint32_t i = 0; i < count; i += 4) {
        for (std::uint32_t lane = 0; lane < 4; ++lane) {
            dest[i + lane] = (1.0f - fade[lane]) * from[i + lane] + fade[lane] * to[i + lane];
            fade[lane] += step * 4.0f;
        }
    }
}

static PitchShiftStep Stretch(Ngs2PitchShiftChannel& channel, const float* input, float* output, float ratio) {
    PitchShiftStep step{};
    if (channel.copyRemaining != 0) {
        step.consumed = std::min(channel.copyRemaining, 960u);
        step.produced = step.consumed;
        channel.copyRemaining -= step.consumed;
        std::copy_n(input, step.produced, output);
    } else if (std::abs(ratio - 1.0f) < 0.01f) {
        step = {960, 960, 960, 960};
        std::copy_n(input, 960, output);
    } else if (ratio < 1.0f) {
        const auto overlap = FindOverlap(input, input, 120, 952);
        if (overlap == 0) throw std::runtime_error("NGS2: PitchShift could not select a pitch-down overlap");
        Crossfade(input, input + overlap, output, overlap);
        const auto duration = static_cast<std::uint32_t>(((ratio * static_cast<float>(overlap)) / (1.0f - ratio)) + 0.5f);
        channel.copyRemaining = duration - overlap;
        step = {overlap * 2, overlap, overlap + duration, duration};
    } else {
        const auto overlap = FindOverlap(channel.overlapHistory.data() + 840, input, 0, 832);
        const auto length = overlap + 120;
        Crossfade(input, channel.overlapHistory.data() + 960 - length, output, length);
        const auto duration = static_cast<std::uint32_t>((static_cast<float>(length) / (ratio - 1.0f)) + 0.5f);
        channel.copyRemaining = duration;
        return {0, length, duration, duration + length};
    }
    if (step.consumed >= 960) {
        std::copy_n(input + step.consumed - 960, 960, channel.overlapHistory.begin());
    } else if (step.consumed != 0) {
        std::move(channel.overlapHistory.begin() + step.consumed, channel.overlapHistory.end(), channel.overlapHistory.begin());
        std::copy_n(input, step.consumed, channel.overlapHistory.end() - step.consumed);
    }
    return step;
}

static const std::array<std::array<float, 8>, 256>& ResampleKernel() {
    static const auto table = [] {
        std::array<std::array<float, 8>, 256> result{};
        for (std::uint32_t phase = 0; phase < 256; ++phase) {
            for (std::uint32_t tap = 0; tap < 8; ++tap) {
                const double x = static_cast<double>(tap) - 4.0 + static_cast<double>(phase) / 256.0;
                const double sinc = x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
                result[phase][tap] = static_cast<float>(sinc * (0.5 + 0.5 * std::cos(std::numbers::pi * x / 4.0)));
            }
        }
        return result;
    }();
    return table;
}

static void Resample(Ngs2PitchShiftChannel& channel, float* dest, std::uint32_t consumed) {
    std::array<float, 1095> scratch{};
    std::copy(channel.resampleHistory.begin(), channel.resampleHistory.end(), scratch.begin());
    std::copy_n(channel.output.begin(), consumed, scratch.begin() + 7);
    std::copy_n(scratch.begin() + consumed, 7, channel.resampleHistory.begin());
    const float step = static_cast<float>(consumed) / 64.0f;
    const auto& kernel = ResampleKernel();
    for (std::uint32_t i = 0; i < 64; ++i) {
        const float position = static_cast<float>(i) * step;
        const auto index = static_cast<std::uint32_t>(position);
        const float phase = ((static_cast<float>(index) - position) + 1.0f) * 256.0f;
        const auto phaseIndex = static_cast<std::uint32_t>(phase);
        const float fraction = phase - static_cast<float>(phaseIndex);
        std::array<float, 4> lanes{};
        for (std::uint32_t lane = 0; lane < 4; ++lane) {
            const auto coefficient = [&](std::uint32_t tap) {
                return (1.0f - fraction) * kernel[phaseIndex & 255u][tap] + fraction * kernel[(phaseIndex + 1) & 255u][tap];
            };
            lanes[lane] = coefficient(lane) * scratch[index + lane] + coefficient(lane + 4) * scratch[index + lane + 4];
        }
        dest[i] = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
    }
}

void Ngs2PitchShiftChannel::Process(const float* source, float* dest, float ratio) {
    for (std::uint32_t i = 0; i < 64; ++i) {
        filterHistory[filterCursor] = source[i];
        filterCursor = (filterCursor + 1) % 142;
    }
    for (std::uint32_t i = 0; i < 64; ++i) input[inputCount + i] = FilterSample(*this, (filterCursor + 64 + i) % 142);
    inputCount += 64;
    if (inputCount <= 1984 && !started) {
        std::fill_n(dest, 64, 0.0f);
        return;
    }
    started = true;
    while (outputCount == 0 || outputCount * inputTime < outputTime * 64) {
        const auto step = Stretch(*this, input.data(), output.data() + outputCount, ratio);
        inputCount -= step.consumed;
        std::move(input.begin() + step.consumed, input.end(), input.begin());
        outputCount += step.produced;
        inputTime += step.inputTime;
        outputTime += step.outputTime;
    }
    const auto consumed = outputTime * 64 / inputTime;
    inputTime -= 64;
    outputTime -= consumed;
    Resample(*this, dest, consumed);
    outputCount -= consumed;
    std::move(output.begin() + consumed, output.end(), output.begin());
}

bool Ngs2HasPitchShift(const Ngs2Voice& voice) {
    return std::any_of(voice.customPitchShift.begin(), voice.customPitchShift.end(), [](const auto& state) { return !state.channels.empty(); });
}

void Ngs2ResetPitchShift(Ngs2Voice& voice) {
    for (auto& state : voice.customPitchShift) {
        state.bypassMask = ~0u;
        state.ratio = 1.0f;
        state.reset = true;
        state.changed = false;
        state.initializedChannels = 0;
    }
    voice.outputTransition = Ngs2VoiceTransition::None;
}

void Ngs2RestartPitchShift(Ngs2Voice& voice) {
    for (auto& state : voice.customPitchShift) {
        state.reset = true;
        state.initializedChannels = 0;
    }
}

void Ngs2ProcessPitchShift(Ngs2Voice& voice, std::size_t module, std::uint32_t grain) {
    if (voice.channels >= 32) throw std::runtime_error("NGS2: PitchShift channel masks for 32 or more channels are not implemented");
    auto& state = voice.customPitchShift[module];
    const auto active = ((1u << voice.channels) - 1) & ~state.bypassMask;
    if (active == 0) return;
    if (grain == 0 || grain % 64 != 0) throw std::runtime_error("NGS2: PitchShift requires a grain divisible by 64");
    if (!state.reset && (active & ~state.initializedChannels) != 0) {
        throw std::runtime_error("NGS2: enabling a PitchShift channel bypassed at initialization is not implemented");
    }
    for (std::uint32_t c = 0; c < voice.channels; ++c) {
        if ((active & (1u << c)) == 0) continue;
        auto& channel = state.channels[c];
        if (state.reset) channel = {};
        if (state.reset || state.changed) channel.SetPitch(state.ratio);
        auto* samples = voice.samples.data() + static_cast<std::size_t>(c) * grain;
        for (std::uint32_t i = 0; i < grain; i += 64) channel.Process(samples + i, samples + i, state.ratio);
    }
    state.initializedChannels |= active;
    state.reset = false;
    state.changed = false;
    voice.hasSamples = std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
}

void Ngs2ApplyVoiceGain(Ngs2Voice& voice, std::uint32_t grain) {
    if (voice.IsMastering()) {
        voice.outputTransition = Ngs2VoiceTransition::None;
        return;
    }
    if (!Ngs2HasPitchShift(voice) && !voice.IsSampler() && !voice.IsMixer()) return;
    const auto transition = voice.outputTransition;
    const bool paused = voice.state == Ngs2PlayState::Paused || (voice.stateFlags & SCE_NGS2_VOICE_STATE_FLAG_PAUSED) != 0;
    if (transition == Ngs2VoiceTransition::None && !paused) return;
    if (transition != Ngs2VoiceTransition::None && grain < 64) throw std::runtime_error("NGS2: voice transitions require at least 64 samples");
    for (std::uint32_t c = 0; c < voice.channels; ++c) {
        auto* samples = voice.samples.data() + static_cast<std::size_t>(c) * grain;
        for (std::uint32_t i = 0; i < grain; ++i) {
            float gain = 1.0f;
            if (transition == Ngs2VoiceTransition::Pause) {
                if (i >= grain - 64) gain = static_cast<float>(grain - i) / 64.0f;
            } else if (transition == Ngs2VoiceTransition::Resume) {
                if (i < 64) gain = static_cast<float>(i) / 64.0f;
            } else if (transition == Ngs2VoiceTransition::Stop) {
                gain = i < 64 ? 1.0f - static_cast<float>(i) / 64.0f : 0.0f;
            } else if (transition == Ngs2VoiceTransition::Kill || paused) {
                gain = 0.0f;
            }
            samples[i] *= gain;
        }
    }
    voice.outputTransition = Ngs2VoiceTransition::None;
    voice.hasSamples = std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
}
