#include "Ngs2Test.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

struct Biquad {
    double b0, b1, b2, a1, a2;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    Biquad(double frequency, double q, double level, double rate) {
        const double w = 2.0 * std::numbers::pi * frequency / rate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - std::cos(w)) / 2.0 / a0 * level;
        b1 = (1.0 - std::cos(w)) / a0 * level;
        b2 = b0;
        a1 = -2.0 * std::cos(w) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    float Next(double x) {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return static_cast<float>(y);
    }
};

constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;

Ngs2SamplerVoiceFilterParam Filter(std::uint32_t index, std::uint32_t location, std::uint32_t type, std::uint64_t mask,
                                   float frequency, float q, float level) {
    Ngs2SamplerVoiceFilterParam param{};
    param.header = {sizeof(param), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_FILTER};
    param.index = index;
    param.location = location;
    param.type = type;
    param.channel_mask = mask;
    param.frequency = frequency;
    param.q = q;
    param.level = level;
    return param;
}

void SetFilter(uintptr_t voice, const Ngs2SamplerVoiceFilterParam& param) {
    RequireEqual(sceNgs2VoiceControl(voice, &param.header), SCE_NGS2_OK, "set filter " + std::to_string(param.index));
}

uintptr_t Sampler(Ngs2Fixture& ngs2, uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t channels,
                  std::uint32_t rate) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, rate, 0, 0, 0}});
    const auto frames = static_cast<std::uint32_t>(pcm.size() / channels);
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, frames, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

std::vector<float> RenderGrain(uintptr_t system, std::uint32_t channels) {
    std::vector<float> out(Grain * channels, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
    return out;
}

void RequireSilence(const std::vector<float>& out, const char* message) {
    for (std::size_t i = 0; i < out.size(); ++i) RequireEqual(out[i], 0.0f, std::string(message) + " sample " + std::to_string(i));
}

const Case lowPass{"SamplerFilter_LowPass_MatchesBiquadAndRingsPastData", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 1);
    std::vector<std::int16_t> pcm(Grain * 2, 0);
    pcm[0] = 16384;
    pcm[5] = -8192;
    const auto sampler = Sampler(ngs2, system, pcm, 1, 48000);
    Patch(sampler, master);
    const auto param = Filter(7, 1, 1, 0, 1000.0f, 0.70710678f, 0.75f);
    SetFilter(sampler, param);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    Biquad reference(1000.0, static_cast<double>(0.70710678f), 0.75, 48000.0);
    for (std::uint32_t grain = 0; grain < 4; grain++) {
        const auto out = RenderGrain(system, 1);
        for (std::uint32_t i = 0; i < Grain; i++) {
            const auto frame = grain * Grain + i;
            const float expected = reference.Next(frame < pcm.size() ? pcm[frame] / 32768.0 : 0.0);
            Require(std::abs(out[i] - expected) <= 1e-7f, "grain " + std::to_string(grain) + " sample " + std::to_string(i) +
                                                            ": expected " + std::to_string(expected) + ", got " +
                                                            std::to_string(out[i]));
        }
        Require(out[Grain - 1] != 0.0f, "filter tail rings in grain " + std::to_string(grain));
        RequireEqual(Flags(sampler), playing, "playing in grain " + std::to_string(grain));
        SetFilter(sampler, param);
    }
}};

const Case filterDisabled{"SamplerFilter_DisabledAfterTail_StopsVoice", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 1);
    std::vector<std::int16_t> pcm(Grain * 2, 0);
    pcm[0] = 16384;
    pcm[5] = -8192;
    const auto sampler = Sampler(ngs2, system, pcm, 1, 48000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(7, 1, 1, 0, 1000.0f, 0.70710678f, 0.75f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    for (std::uint32_t grain = 0; grain < 4; grain++) RenderGrain(system, 1);
    RequireEqual(Flags(sampler), playing, "playing while the filter rings");
    SetFilter(sampler, Filter(7, 0, 0, 0, NAN, 0.0f, -1.0f));
    RequireSilence(RenderGrain(system, 1), "after disabling");
    RequireEqual(Flags(sampler), 0u, "stopped after disabling");
}};

const Case bypassMask{"SamplerFilter_ChannelMaskBypass_FiltersOnlyUnmaskedChannelGain", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 2);
    std::vector<std::int16_t> pcm;
    for (std::uint32_t i = 0; i < Grain; i++) {
        pcm.push_back(static_cast<std::int16_t>(i * 1000));
        pcm.push_back(static_cast<std::int16_t>(-4000 + i * 700));
    }
    const auto sampler = Sampler(ngs2, system, pcm, 2, 48000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(0, 1, 1, 1, 24000.0f, 1.0f, 0.5f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    const auto out = RenderGrain(system, 2);
    for (std::uint32_t i = 0; i < Grain; i++) {
        RequireEqual(out[i * 2], pcm[i * 2] / 32768.0f, "bypassed channel frame " + std::to_string(i));
        RequireEqual(out[i * 2 + 1], pcm[i * 2 + 1] / 32768.0f * 0.5f, "filtered channel frame " + std::to_string(i));
    }
    RequireEqual(Flags(sampler), playing, "playing");
    RequireSilence(RenderGrain(system, 2), "after the data");
    RequireEqual(Flags(sampler), 0u, "stopped");
}};

const Case systemRate{"SamplerFilter_CutoffAboveSourceNyquist_UsesSystemRateAndZeroCutoffSilences", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 1);
    const std::vector<std::int16_t> pcm(Grain * 2, 8192);
    const auto sampler = Sampler(ngs2, system, pcm, 1, 24000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(0, 1, 1, 0, 18000.0f, 0.70710678f, 1.0f));
    SetFilter(sampler, Filter(1, 1, 1, 0, 30000.0f, 0.70710678f, 1.0f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    Biquad reference(18000.0, static_cast<double>(0.70710678f), 1.0, 48000.0);
    const auto out = RenderGrain(system, 1);
    for (std::uint32_t i = 0; i < Grain; i++) {
        const float expected = reference.Next(0.25);
        Require(std::abs(out[i] - expected) <= 1e-7f,
                "sample " + std::to_string(i) + ": expected " + std::to_string(expected) + ", got " + std::to_string(out[i]));
    }
    SetFilter(sampler, Filter(1, 1, 1, 0, 0.0f, 0.70710678f, 1.0f));
    RequireSilence(RenderGrain(system, 1), "zero cutoff");
}};

const Case setupClearsFilters{"SamplerFilter_VoiceSetupAgain_ClearsFilters", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 1);
    const std::vector<std::int16_t> pcm(Grain * 2, 8192);
    const auto sampler = Sampler(ngs2, system, pcm, 1, 24000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(0, 1, 1, 0, 18000.0f, 0.70710678f, 1.0f));
    SetFilter(sampler, Filter(1, 1, 1, 0, 0.0f, 0.70710678f, 1.0f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSilence(RenderGrain(system, 1), "zero cutoff");

    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto out = RenderGrain(system, 1);
    for (std::size_t i = 0; i < out.size(); ++i) RequireEqual(out[i], 0.25f, "unfiltered sample " + std::to_string(i));
}};

template<typename TError>
void RequireRejected(uintptr_t voice, const Ngs2SamplerVoiceFilterParam& param, const char* message) {
    RequireThrows<TError>([&] { sceNgs2VoiceControl(voice, &param.header); }, message);
}

const Case rejectedParams{"SamplerFilter_InvalidOrUnsupportedParams_AreRejected", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const std::vector<std::int16_t> silence(Grain, 0);
    const auto sampler = Sampler(ngs2, system, silence, 1, 48000);
    RequireRejected<std::invalid_argument>(sampler, Filter(8, 1, 1, 0, 1000.0f, 1.0f, 1.0f), "index 8");
    RequireRejected<std::runtime_error>(sampler, Filter(0, 0, 1, 0, 1000.0f, 1.0f, 1.0f), "location 0 with a type");
    RequireRejected<std::runtime_error>(sampler, Filter(0, 1, 2, 0, 1000.0f, 1.0f, 1.0f), "type 2");
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, NAN, 1.0f, 1.0f), "NaN frequency");
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, -1.0f, 1.0f, 1.0f), "negative frequency");
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, 1000.0f, 0.0f, 1.0f), "zero q");
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, 1000.0f, 1.0f, -0.5f), "negative level");
    auto small = Filter(0, 1, 1, 0, 1000.0f, 1.0f, 1.0f);
    small.header.size = sizeof(small) - 4;
    RequireRejected<std::invalid_argument>(sampler, small, "short header size");
}};

const Case rateChange{"SystemSetSampleRate_EnabledFilter_RejectsChangeUntilDisabled", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const std::vector<std::int16_t> silence(Grain, 0);
    const auto sampler = Sampler(ngs2, system, silence, 1, 48000);
    SetFilter(sampler, Filter(0, 1, 1, 0, 1000.0f, 1.0f, 1.0f));
    RequireEqual(sceNgs2SystemSetSampleRate(system, 48000), SCE_NGS2_OK, "unchanged rate");
    RequireThrows<std::runtime_error>([&] { sceNgs2SystemSetSampleRate(system, 96000); }, "changed rate under a filter");
    SetFilter(sampler, Filter(0, 1, 0, 0, 0.0f, 1.0f, 1.0f));
    RequireEqual(sceNgs2SystemSetSampleRate(system, 96000), SCE_NGS2_OK, "changed rate after disabling");
}};

} // namespace
