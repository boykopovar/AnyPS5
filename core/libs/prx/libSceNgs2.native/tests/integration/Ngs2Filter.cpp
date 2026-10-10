#include "Ngs2Test.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>
#include <stdexcept>
#include <vector>

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

static Ngs2SamplerVoiceFilterParam Filter(std::uint32_t index, std::uint32_t location, std::uint32_t type, std::uint64_t mask, float frequency, float q, float level) {
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

static void SetFilter(uintptr_t voice, const Ngs2SamplerVoiceFilterParam& param) {
    Require(sceNgs2VoiceControl(voice, &param.header) == SCE_NGS2_OK);
}

static uintptr_t Sampler(uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t channels, std::uint32_t rate) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, rate, 0, 0, 0}});
    const auto frames = static_cast<std::uint32_t>(pcm.size() / channels);
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, frames, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

static std::vector<float> RenderGrain(uintptr_t system, std::uint32_t channels) {
    std::vector<float> out(Grain * channels, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    return out;
}

static void TestLowPassAndTail() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    std::vector<std::int16_t> pcm(Grain * 2, 0);
    pcm[0] = 16384;
    pcm[5] = -8192;
    const auto sampler = Sampler(system, pcm, 1, 48000);
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
            Require(std::abs(out[i] - expected) <= 1e-7f);
        }
        Require(out[Grain - 1] != 0.0f);
        Require(Flags(sampler) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
        SetFilter(sampler, param);
    }

    SetFilter(sampler, Filter(7, 0, 0, 0, NAN, 0.0f, -1.0f));
    for (float sample : RenderGrain(system, 1)) Require(sample == 0.0f);
    Require(Flags(sampler) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestBypassMaskAndGain() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    std::vector<std::int16_t> pcm;
    for (std::uint32_t i = 0; i < Grain; i++) {
        pcm.push_back(static_cast<std::int16_t>(i * 1000));
        pcm.push_back(static_cast<std::int16_t>(-4000 + i * 700));
    }
    const auto sampler = Sampler(system, pcm, 2, 48000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(0, 1, 1, 1, 24000.0f, 1.0f, 0.5f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    auto out = RenderGrain(system, 2);
    for (std::uint32_t i = 0; i < Grain; i++) {
        Require(out[i * 2] == pcm[i * 2] / 32768.0f);
        Require(out[i * 2 + 1] == pcm[i * 2 + 1] / 32768.0f * 0.5f);
    }
    Require(Flags(sampler) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    out = RenderGrain(system, 2);
    for (float sample : out) Require(sample == 0.0f);
    Require(Flags(sampler) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSystemRateAndZeroCutoff() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm(Grain * 2, 8192);
    const auto sampler = Sampler(system, pcm, 1, 24000);
    Patch(sampler, master);
    SetFilter(sampler, Filter(0, 1, 1, 0, 18000.0f, 0.70710678f, 1.0f));
    SetFilter(sampler, Filter(1, 1, 1, 0, 30000.0f, 0.70710678f, 1.0f));
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    Biquad reference(18000.0, static_cast<double>(0.70710678f), 1.0, 48000.0);
    const auto out = RenderGrain(system, 1);
    for (std::uint32_t i = 0; i < Grain; i++) Require(std::abs(out[i] - reference.Next(0.25)) <= 1e-7f);

    SetFilter(sampler, Filter(1, 1, 1, 0, 0.0f, 0.70710678f, 1.0f));
    for (float sample : RenderGrain(system, 1)) Require(sample == 0.0f);

    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    for (float sample : RenderGrain(system, 1)) Require(sample == 0.25f);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

template <typename TError>
static void RequireRejected(uintptr_t voice, const Ngs2SamplerVoiceFilterParam& param) {
    bool rejected = false;
    try {
        sceNgs2VoiceControl(voice, &param.header);
    } catch (const TError&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestRejectedParams() {
    const auto system = CreateSystem();
    const auto sampler = Sampler(system, std::vector<std::int16_t>(Grain, 0), 1, 48000);
    RequireRejected<std::invalid_argument>(sampler, Filter(8, 1, 1, 0, 1000.0f, 1.0f, 1.0f));
    RequireRejected<std::runtime_error>(sampler, Filter(0, 0, 1, 0, 1000.0f, 1.0f, 1.0f));
    RequireRejected<std::runtime_error>(sampler, Filter(0, 1, 2, 0, 1000.0f, 1.0f, 1.0f));
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, NAN, 1.0f, 1.0f));
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, -1.0f, 1.0f, 1.0f));
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, 1000.0f, 0.0f, 1.0f));
    RequireRejected<std::invalid_argument>(sampler, Filter(0, 1, 1, 0, 1000.0f, 1.0f, -0.5f));
    auto small = Filter(0, 1, 1, 0, 1000.0f, 1.0f, 1.0f);
    small.header.size = sizeof(small) - 4;
    RequireRejected<std::invalid_argument>(sampler, small);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRateChangeUnderFilter() {
    const auto system = CreateSystem();
    const auto sampler = Sampler(system, std::vector<std::int16_t>(Grain, 0), 1, 48000);
    SetFilter(sampler, Filter(0, 1, 1, 0, 1000.0f, 1.0f, 1.0f));
    Require(sceNgs2SystemSetSampleRate(system, 48000) == SCE_NGS2_OK);
    bool rejected = false;
    try {
        sceNgs2SystemSetSampleRate(system, 96000);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
    SetFilter(sampler, Filter(0, 1, 0, 0, 0.0f, 1.0f, 1.0f));
    Require(sceNgs2SystemSetSampleRate(system, 96000) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    TestLowPassAndTail();
    TestBypassMaskAndGain();
    TestSystemRateAndZeroCutoff();
    TestRejectedParams();
    TestRateChangeUnderFilter();
    return 0;
}
