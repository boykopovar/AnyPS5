#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr std::uint32_t TAP_RING_SAMPLES = 14400;
static constexpr std::uint32_t EARLY_TAPS = 4;
static constexpr std::uint32_t COMBS = 16;
static constexpr std::uint32_t ALLPASSES = 4;
static constexpr std::uint32_t LFE_CHANNEL = 3;

static constexpr float TAP_TIMES[8][EARLY_TAPS] = {
    {0, 11, 23, 35}, {0, 14.75f, 26.5499992f, 38.0299988f}, {0, 6, 17, 29}, {0, 8, 20, 53},
    {0, 11, 17, 30}, {0, 16, 30, 55}, {0, 20, 29, 40}, {0, 37, 47, 62},
};
static constexpr float TAP_GAINS[8][EARLY_TAPS] = {
    {0.7f, 0.4f, 0.134f, 0.1f}, {0.7f, 0.4f, 0.3f, 0.1f}, {0.7f, 0.45f, -0.6f, 0.2f}, {0.7f, 0.55f, -0.6f, 0.2f},
    {0.7f, -0.45f, 0.6f, 0.3f}, {0.7f, 0.7f, 0.7f, 0.4f}, {0.7f, 0.7f, -0.4f, 0.3f}, {0.7f, 0.7f, -0.4f, 0.4f},
};
static constexpr float COMB_TIMES[COMBS] = {
    22.0100002f, 23.25f, 24.75f, 26.6000004f, 28.25f, 29.6299992f, 31.0599995f, 32.4399986f,
    33.6899986f, 34.9500008f, 36.2900009f, 37.6500015f, 38.9900017f, 40.3499985f, 41.6699982f, 43.0499992f,
};
static constexpr float ALLPASS_TIMES[ALLPASSES] = {11.5799999f, 9.18999958f, 7.0999999f, 4.69000006f};
static constexpr std::int32_t ALLPASS_OFFSETS[8][ALLPASSES] = {
    {0, -32, -6, -11}, {-10, -5, 0, -34}, {-8, -3, -33, -5}, {-7, -9, -5, -8},
    {-33, -8, -8, 0}, {-11, -21, -10, -7}, {-9, -10, -27, -3}, {-6, -11, -9, -24},
};
static constexpr std::uint16_t CHANNEL_COMBS[8] = {0xaaaa, 0x5555, 0xcccc, 0x3333, 0x0ff0, 0x9ab2, 0x3c3c, 0x399c};
static constexpr Ngs2ReverbI3DL2Param SETUP_PARAMS{1.0f, 0.0f, -10000, 0, 0, 1.0f, 0.5f, -10000, 0.02f, -10000, 0.04f, 100.0f, 100.0f, 5000.0f, {}};

struct Ngs2ReverbTaps {
    std::array<std::uint32_t, EARLY_TAPS + 1> delays{};
    std::array<float, EARLY_TAPS + 1> gains{};
};

struct Ngs2Comb {
    std::vector<float> buffer;
    std::uint32_t cursor = 0;
    float feedback = 0.0f;
    float lowpassIn = 0.0f;
    float lowpassBack = 0.0f;
    float direct = 0.0f;
    float filtered = 0.0f;
    float emphasis = 0.0f;
    float emphasisBack = 0.0f;
    float previous = 0.0f;
    float lowpass = 0.0f;
};

struct Ngs2Allpass {
    std::vector<float> buffer;
    std::uint32_t cursor = 0;
};

struct Ngs2ReverbChannel {
    std::array<Ngs2Allpass, ALLPASSES> allpasses;
    float y1 = 0.0f;
    float y2 = 0.0f;
    float wet = 0.0f;
    float dry = 0.0f;
};

struct Ngs2ReverbState {
    Ngs2ReverbI3DL2Param params{};
    bool reset = true;
    bool changed = true;
    std::uint32_t sampleRate = 0;
    std::vector<float> ring;
    std::uint32_t ringCursor = 0;
    Ngs2ReverbTaps taps;
    float earlyGain = 0.0f;
    float lateGain = 0.0f;
    float density = NAN;
    std::array<Ngs2Comb, COMBS> combs;
    std::array<Ngs2ReverbChannel, 8> channels;
    bool roomFilter = false;
    float roomB0 = 0.0f;
    float roomA1 = 0.0f;
    float roomA2 = 0.0f;
};

void Ngs2ReverbDeleter::operator()(Ngs2ReverbState* reverb) const {
    delete reverb;
}

static float Millibels(float millibels) {
    if (millibels < -10000.0f) return 0.0f;
    return static_cast<float>(std::pow(10.0, static_cast<double>(millibels * 0.0005f)));
}

static float Clamp(float value, float low, float high) {
    return value < low ? low : std::min(high, value);
}

static std::int32_t ClampMillibels(std::int32_t value, std::int32_t high) {
    return std::max<std::int32_t>(-10000, std::min(value, high));
}

static std::uint32_t Samples(float milliseconds, float rate) {
    return static_cast<std::uint32_t>(rate * milliseconds * 0.001f + 0.5f);
}

static void Ramp(float* samples, float from, float to, std::uint32_t grain, const float* source, bool accumulate) {
    const float step = (to - from) / static_cast<float>(grain);
    float gain = from;
    for (std::uint32_t i = 0; i < grain; i++) {
        const float value = source[i] * (from == to ? to : gain);
        samples[i] = accumulate ? samples[i] + value : value;
        gain += step;
    }
}

static void RunTaps(const Ngs2ReverbState& reverb, const Ngs2ReverbTaps& taps, const float* input, std::uint32_t grain, float* early, float* late) {
    const auto size = static_cast<std::uint32_t>(reverb.ring.size());
    std::fill(early, early + grain, 0.0f);
    std::fill(late, late + grain, 0.0f);
    for (std::uint32_t tap = 0; tap <= EARLY_TAPS; tap++) {
        float* out = tap < EARLY_TAPS ? early : late;
        const float gain = taps.gains[tap];
        const std::uint32_t delay = taps.delays[tap];
        std::uint32_t read = (reverb.ringCursor + size - delay) % size;
        for (std::uint32_t i = 0; i < grain; i++) {
            if (i < delay) {
                out[i] += reverb.ring[read] * gain;
                read = read + 1 == size ? 0 : read + 1;
            } else {
                out[i] += input[i - delay] * gain;
            }
        }
    }
}

static Ngs2ReverbTaps TapsFor(const Ngs2ReverbI3DL2Param& params, std::uint32_t size, float rate) {
    Ngs2ReverbTaps taps;
    const std::uint32_t pattern = params.reflection_pattern < 8 ? params.reflection_pattern : params.reflection_pattern - 8;
    const auto early = std::min(static_cast<std::uint32_t>(params.reflections_delay * rate * 1000.0f * 0.001f + 0.5f), size);
    const auto late = std::min(static_cast<std::uint32_t>((params.reflections_delay + params.reverb_delay) * rate * 1000.0f * 0.001f + 0.5f), size);
    const auto last = static_cast<std::uint32_t>(rate * TAP_TIMES[pattern][EARLY_TAPS - 1] * 0.001f + 0.5f);
    const std::uint32_t limit = params.reflection_pattern > 7 ? size : late;
    const float scale = early + last > limit ? static_cast<float>(limit - early) / static_cast<float>(last) : 1.0f;
    taps.delays[0] = early;
    taps.gains[0] = TAP_GAINS[pattern][0];
    for (std::uint32_t tap = 1; tap < EARLY_TAPS; tap++) {
        taps.delays[tap] = static_cast<std::uint32_t>(scale * TAP_TIMES[pattern][tap] * rate * 0.001f + 0.5f) + early;
        taps.gains[tap] = taps.delays[tap] == taps.delays[tap - 1] ? 0.0f : TAP_GAINS[pattern][tap];
    }
    taps.delays[EARLY_TAPS] = std::min(early + static_cast<std::uint32_t>(params.reverb_delay * rate * 1000.0f * 0.001f + 0.5f), size);
    taps.gains[EARLY_TAPS] = 1.0f;
    return taps;
}

static void SetupComb(Ngs2Comb& comb, std::uint32_t length, const Ngs2ReverbI3DL2Param& params, float rate, bool clear) {
    if (clear || comb.buffer.size() != length) {
        comb.buffer.assign(length, 0.0f);
        if (comb.cursor >= length) comb.cursor = 0;
    }
    comb.feedback = std::pow(10.0f, static_cast<float>(length) / rate / params.decay_time * -3.0f);
    const float ratio = params.decay_hf_ratio;
    if (ratio > 1.0f) {
        const float high = std::pow(10.0f, (ratio - 1.0f) * std::log10(comb.feedback));
        comb.lowpassIn = 0.0506209731f;
        comb.lowpassBack = 0.949379027f;
        comb.direct = 1.0f;
        comb.filtered = 1.0f - high;
    } else {
        float shaped = ratio * ratio;
        if (0.8f > ratio) {
            shaped = (0.8f - ratio) * -0.857142925f + 0.8f;
            shaped *= shaped;
        }
        const float back = std::min(0.99977994f, std::log10(comb.feedback) * 2.30258512f * 0.25f * (1.0f - 1.0f / shaped));
        comb.lowpassIn = 1.0f - back;
        comb.lowpassBack = back;
        comb.direct = 0.0f;
        comb.filtered = 1.0f;
    }
    const float root = std::sqrt(ratio);
    const float tilt = (1.0f - root) / (root + 1.0f);
    comb.emphasis = tilt / (1.0f - tilt) + 1.0f;
    comb.emphasisBack = -tilt / (1.0f - tilt);
}

static void Reset(Ngs2ReverbState& reverb) {
    reverb.ring.assign(TAP_RING_SAMPLES, 0.0f);
    reverb.ringCursor = 0;
    reverb.taps = {};
    reverb.earlyGain = 1.0f;
    reverb.lateGain = 1.0f;
    reverb.density = NAN;
    reverb.combs = {};
    reverb.channels = {};
}

static void ApplyParams(Ngs2ReverbState& reverb, float rate) {
    const auto& params = reverb.params;
    reverb.taps = TapsFor(params, static_cast<std::uint32_t>(reverb.ring.size()), rate);
    const float room = std::min(1.0f, Millibels(static_cast<float>(params.room)));
    reverb.earlyGain = Millibels(static_cast<float>(ClampMillibels(params.reflections, 1000))) * room;
    reverb.lateGain = Millibels(static_cast<float>(ClampMillibels(params.reverb, 2000) - 2400)) * room;
    const bool densityChanged = !(reverb.density == params.density);
    reverb.density = params.density;
    const float densityScale = params.density * -3.0f / 100.0f + 4.0f;
    for (std::uint32_t i = 0; i < COMBS; i++) {
        auto length = static_cast<std::uint32_t>(rate * COMB_TIMES[i] / densityScale * 0.001f + 0.5f);
        length += ~length & 1;
        SetupComb(reverb.combs[i], length, params, rate, densityChanged);
    }
    for (std::uint32_t channel = 0; channel < 8; channel++) {
        for (std::uint32_t stage = 0; stage < ALLPASSES; stage++) {
            auto& allpass = reverb.channels[channel].allpasses[stage];
            const auto signedLength = static_cast<std::int32_t>(Samples(ALLPASS_TIMES[stage], rate)) + ALLPASS_OFFSETS[channel][stage];
            if (signedLength < 1) throw std::runtime_error("NGS2: reverb at a sample rate of " + std::to_string(reverb.sampleRate) + " Hz is not implemented");
            const auto length = static_cast<std::uint32_t>(signedLength);
            if (allpass.buffer.size() != length) allpass.buffer.assign(length, 0.0f);
            if (allpass.cursor >= length) allpass.cursor = 0;
        }
    }
}

static void UpdateRoomFilter(Ngs2ReverbState& reverb, float rate) {
    const auto& params = reverb.params;
    const float roomHf = Millibels(static_cast<float>(ClampMillibels(params.room_hf, 0)));
    reverb.roomFilter = 1.0f > roomHf;
    if (!reverb.roomFilter) return;
    const float cosine = std::cos(params.hf_reference * 6.28318548f / rate);
    const float root = std::sqrt((roomHf + roomHf) * (1.0f - cosine) - roomHf * roomHf * (1.0f - cosine * cosine));
    const float pole = ((1.0f - roomHf * cosine) - root) / (1.0f - roomHf);
    reverb.roomB0 = (1.0f - pole) * (1.0f - pole);
    reverb.roomA1 = pole + pole;
    reverb.roomA2 = pole * -pole;
}

static void RunComb(Ngs2Comb& comb, const float* input, float* output, std::uint32_t grain) {
    const auto length = static_cast<std::uint32_t>(comb.buffer.size());
    for (std::uint32_t i = 0; i < grain; i++) {
        const float emphasized = input[i] * comb.emphasis + comb.previous * comb.emphasisBack;
        comb.previous = input[i];
        const float delayed = comb.buffer[comb.cursor];
        comb.lowpass = delayed * comb.lowpassIn + comb.lowpass * comb.lowpassBack;
        const float back = (delayed * comb.direct + comb.lowpass * comb.filtered) * comb.feedback;
        comb.buffer[comb.cursor] = emphasized + back;
        output[i] = back;
        comb.cursor = comb.cursor + 1 == length ? 0 : comb.cursor + 1;
    }
}

static void RunAllpass(Ngs2Allpass& allpass, float gain, float* samples, std::uint32_t grain) {
    const auto length = static_cast<std::uint32_t>(allpass.buffer.size());
    for (std::uint32_t i = 0; i < grain; i++) {
        const float delayed = allpass.buffer[allpass.cursor];
        const float stored = delayed * gain + samples[i];
        allpass.buffer[allpass.cursor] = stored;
        samples[i] = delayed - gain * stored;
        allpass.cursor = allpass.cursor + 1 == length ? 0 : allpass.cursor + 1;
    }
}

static void MixDown(const float* input, std::uint32_t channels, std::uint32_t grain, float* mono) {
    const float* channel[8];
    for (std::uint32_t c = 0; c < channels; c++) channel[c] = input + static_cast<std::size_t>(c) * grain;
    if (channels == 1) {
        std::copy(channel[0], channel[0] + grain, mono);
        return;
    }
    const float gain = channels == 2 ? 0.707000017f : channels == 6 ? 0.316000015f : 0.26699999f;
    for (std::uint32_t i = 0; i < grain; i++) mono[i] = channel[0][i] * gain;
    for (std::uint32_t c = 1; c < channels; c++) {
        if (c == LFE_CHANNEL) continue;
        for (std::uint32_t i = 0; i < grain; i++) mono[i] += channel[c][i] * gain;
    }
}

bool Ngs2ProcessReverb(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate) {
    auto& reverb = *voice.reverb;
    const float rate = static_cast<float>(sampleRate);
    const std::uint32_t channels = voice.channels;
    if (reverb.sampleRate != sampleRate) {
        reverb.sampleRate = sampleRate;
        reverb.reset = true;
    }
    const bool reset = reverb.reset;
    if (reset) {
        Reset(reverb);
        reverb.changed = true;
    }
    const std::vector<float> input = voice.samples;
    std::vector<float> mono(grain), early(grain), late(grain);
    MixDown(input.data(), channels, grain, mono.data());
    if (reverb.changed) {
        std::vector<float> oldEarly(grain), oldLate(grain);
        RunTaps(reverb, reverb.taps, mono.data(), grain, oldEarly.data(), oldLate.data());
        ApplyParams(reverb, rate);
        RunTaps(reverb, reverb.taps, mono.data(), grain, early.data(), late.data());
        Ramp(early.data(), 0.0f, 1.0f, grain, early.data(), false);
        Ramp(early.data(), 1.0f, 0.0f, grain, oldEarly.data(), true);
        Ramp(late.data(), 0.0f, 1.0f, grain, late.data(), false);
        Ramp(late.data(), 1.0f, 0.0f, grain, oldLate.data(), true);
    } else {
        RunTaps(reverb, reverb.taps, mono.data(), grain, early.data(), late.data());
    }
    for (std::uint32_t i = 0; i < grain; i++) {
        reverb.ring[reverb.ringCursor] = mono[i];
        reverb.ringCursor = reverb.ringCursor + 1 == reverb.ring.size() ? 0 : reverb.ringCursor + 1;
    }
    std::vector<float> combs(static_cast<std::size_t>(COMBS) * grain);
    for (std::uint32_t i = 0; i < COMBS; i++) RunComb(reverb.combs[i], late.data(), combs.data() + static_cast<std::size_t>(i) * grain, grain);
    UpdateRoomFilter(reverb, rate);
    const auto& params = reverb.params;
    bool audible = false;
    for (std::uint32_t channel = 0; channel < channels; channel++) {
        auto& state = reverb.channels[channel];
        float* out = voice.samples.data() + static_cast<std::size_t>(channel) * grain;
        const float* in = input.data() + static_cast<std::size_t>(channel) * grain;
        if (channel == LFE_CHANNEL) {
            std::copy(in, in + grain, out);
        } else {
            std::vector<float> tail(grain, 0.0f);
            std::uint32_t selected = 0;
            for (std::uint32_t i = 0; i < COMBS; i++) {
                if ((CHANNEL_COMBS[channel] >> i & 1) == 0) continue;
                const float weight = (selected & 1) != 0 ? 1.0f : 1.0f / (static_cast<float>(selected) * 0.5f * 20.0f + 40.0f) * (params.density + -100.0f) + 1.0f;
                const float* comb = combs.data() + static_cast<std::size_t>(i) * grain;
                for (std::uint32_t n = 0; n < grain; n++) tail[n] += comb[n] * weight;
                selected++;
            }
            for (auto& allpass : state.allpasses) RunAllpass(allpass, params.diffusion * 0.005f, tail.data(), grain);
            for (std::uint32_t n = 0; n < grain; n++) out[n] = early[n] * reverb.earlyGain + tail[n] * reverb.lateGain;
            for (std::uint32_t n = 0; n < grain; n++) {
                const float y = reverb.roomFilter ? reverb.roomB0 * out[n] + reverb.roomA1 * state.y1 + reverb.roomA2 * state.y2 : out[n];
                state.y2 = state.y1;
                state.y1 = y;
                out[n] = y;
            }
        }
        const float wet = channel == LFE_CHANNEL ? 0.0f : params.wet;
        Ramp(out, reset ? wet : state.wet, wet, grain, out, false);
        Ramp(out, reset ? params.dry : state.dry, params.dry, grain, in, true);
        state.wet = wet;
        state.dry = params.dry;
        for (std::uint32_t n = 0; n < grain && !audible; n++) audible = out[n] != 0.0f;
    }
    reverb.reset = false;
    reverb.changed = false;
    return audible;
}

void Ngs2SetReverbParams(Ngs2Voice& voice, const Ngs2ReverbI3DL2Param& params) {
    if (std::isnan(params.wet) || std::isnan(params.dry) || std::isnan(params.decay_time) || std::isnan(params.decay_hf_ratio) || std::isnan(params.reflections_delay) ||
        std::isnan(params.reverb_delay) || std::isnan(params.diffusion) || std::isnan(params.density) || std::isnan(params.hf_reference)) {
        APS5_INVALID_ARG_EX;
    }
    if (!voice.reverb) voice.reverb.reset(new Ngs2ReverbState{});
    auto& stored = voice.reverb->params;
    stored = params;
    stored.wet = Clamp(params.wet, 0.0f, 32.0f);
    stored.dry = Clamp(params.dry, 0.0f, 32.0f);
    stored.reflection_pattern = params.reflection_pattern < 16 ? params.reflection_pattern : 0;
    stored.decay_time = Clamp(params.decay_time, 0.1f, 20.0f);
    stored.decay_hf_ratio = Clamp(params.decay_hf_ratio, 0.00999999978f, 2.0f);
    stored.reflections_delay = Clamp(params.reflections_delay, 0.0f, 0.300000012f);
    stored.reverb_delay = Clamp(params.reverb_delay, 0.0f, 0.100000001f);
    stored.diffusion = Clamp(params.diffusion, 0.0f, 100.0f);
    stored.density = Clamp(params.density, 0.0f, 100.0f);
    stored.hf_reference = Clamp(params.hf_reference, 20.0f, 20000.0f);
    voice.reverb->changed = true;
}

void Ngs2SetupReverb(Ngs2Voice& voice) {
    voice.reverb.reset(new Ngs2ReverbState{});
    Ngs2SetReverbParams(voice, SETUP_PARAMS);
}

void Ngs2ClearReverb(Ngs2Voice& voice) {
    if (voice.reverb) voice.reverb->reset = true;
}
