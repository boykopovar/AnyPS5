#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "Ngs2Internal.hpp"

#ifdef APS5_NGS2_PS4_LIBRARY
static constexpr bool PS4_LIBRARY = true;
#else
static constexpr bool PS4_LIBRARY = false;
#endif

static std::array<float, 5> FilterCoefficients(const Ngs2SamplerVoiceFilterParam& param, float rate) {
    if (param.type == 11) return {param.frequency, param.q, param.level, std::bit_cast<float>(param.reserved[0]), std::bit_cast<float>(param.reserved[1])};
    const float fc = std::clamp(param.frequency, (PS4_LIBRARY ? 0.001f : 0.0001f) * rate, 0.499f * rate);
    const float w = (fc / rate) * 6.28318548f;
    const auto s = static_cast<float>(std::sin(static_cast<double>(w)));
    const auto c = static_cast<float>(std::cos(static_cast<double>(w)));
    if (param.type == 9) {
        const float inv = 1.0f / (1.0f + (c + s));
        const float g = s * inv;
        return {g, g, 0.0f, (1.0f + (c - s)) * inv, 0.0f};
    }
    if (param.type == 10) {
        const float inv = 1.0f / ((1.0f + c) + s);
        const float g = (1.0f + c) * inv;
        return {g, -g, 0.0f, ((1.0f + c) - s) * inv, 0.0f};
    }
    const float q = std::clamp(param.q, 0.0001f, 32.0f);
    const float alpha = s / (2.0f * q);
    const float level = param.level > 0.0001f ? param.level : 0.0001f;
    const float A = std::sqrt(level);
    const float t = (std::sqrt(A) / q) * s;
    const float pPlus = (A + 1.0f) + (A - 1.0f) * c;
    const float pMinus = (A + 1.0f) - (A - 1.0f) * c;
    const float mPlus = (A - 1.0f) + (A + 1.0f) * c;
    const float mMinus = (A - 1.0f) - (A + 1.0f) * c;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f + alpha, a1 = -2.0f * c, a2 = 1.0f - alpha;
    switch (param.type) {
        case 1: b1 = 1.0f - c; b0 = b1 * 0.5f; b2 = b0; break;
        case 2: b1 = -(1.0f + c); b0 = (1.0f + c) * 0.5f; b2 = b0; break;
        case 3: b0 = s * 0.5f; b2 = -b0; break;
        case 4: b0 = alpha; b2 = -alpha; break;
        case 5: b0 = 1.0f; b1 = -2.0f * c; b2 = 1.0f; break;
        case 6:
            b0 = A * (pMinus + t); b1 = 2.0f * A * mMinus; b2 = A * (pMinus - t);
            a0 = pPlus + t; a1 = -2.0f * mPlus; a2 = pPlus - t;
            break;
        case 7:
            b0 = A * (pPlus + t); b1 = -2.0f * A * mPlus; b2 = A * (pPlus - t);
            a0 = pMinus + t; a1 = 2.0f * mMinus; a2 = pMinus - t;
            break;
        default:
            b0 = 1.0f + alpha * A; b1 = -2.0f * c; b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A; a2 = 1.0f - alpha / A;
            break;
    }
    const float inv = 1.0f / a0;
    return {b0 * inv, b1 * inv, b2 * inv, -(a1 * inv), -(a2 * inv)};
}

void Ngs2ConfigureFilter(Ngs2Filter& filter, const Ngs2SamplerVoiceFilterParam& param, float rate) {
    const auto [i0, i1, i2, o1, o2] = FilterCoefficients(param, rate);
    auto& h = filter.h;
    auto& c = filter.c;
    auto& d = filter.d;
    auto& e1 = filter.e1;
    auto& e2 = filter.e2;
    h[0] = i0;
    h[1] = i1 + o1 * h[0];
    h[2] = (i2 + o2 * h[0]) + o1 * h[1];
    h[3] = o2 * h[1] + o1 * h[2];
    c[0] = o1;
    c[1] = o2 + o1 * c[0];
    d[0] = o2;
    d[1] = o1 * d[0];
    e1[0] = i1;
    e1[1] = i2 + o1 * e1[0];
    e2[0] = i2;
    e2[1] = o1 * e2[0];
    for (int j = 2; j < 4; j++) {
        c[j] = o2 * c[j - 2] + o1 * c[j - 1];
        d[j] = o2 * d[j - 2] + o1 * d[j - 1];
        e1[j] = o2 * e1[j - 2] + o1 * e1[j - 1];
        e2[j] = o2 * e2[j - 2] + o1 * e2[j - 1];
    }
}

static bool Bypasses(const Ngs2Filter& filter, std::uint32_t channel) {
    return channel < 64 && (filter.bypassMask & (std::uint64_t{1} << channel)) != 0;
}

bool Ngs2FilterHasHistory(const Ngs2Filter& filter) {
    if (!filter.enabled) return false;
    for (std::uint32_t channel = 0; channel < filter.history.size(); channel++) {
        const auto& h = filter.history[channel];
        if (!Bypasses(filter, channel) && (h.x1 != 0.0f || h.x2 != 0.0f || h.y1 != 0.0f || h.y2 != 0.0f)) return true;
    }
    return false;
}

void Ngs2ResetFilterHistory(Ngs2Voice& voice) {
    for (auto& filter : voice.filters) std::fill(filter.history.begin(), filter.history.end(), Ngs2FilterHistory{});
}

void Ngs2ApplyFilter(Ngs2Voice& voice, Ngs2Filter& filter, std::uint32_t grain) {
    if (filter.location >= 2) {
        const std::size_t target = filter.location - 2;
        if (target >= voice.ports.size() || voice.ports[target].dest == nullptr) return;
        for (std::size_t port = 0; port < voice.ports.size(); port++) {
            if (port != target && voice.ports[port].dest != nullptr) throw std::runtime_error("NGS2: a PRE_SEND sampler filter on a voice with another patched port is not implemented");
        }
    }
    for (std::uint32_t channel = 0; channel < voice.channels; channel++) {
        if (Bypasses(filter, channel)) continue;
        auto& h = filter.history[channel];
        float* samples = voice.samples.data() + static_cast<std::size_t>(channel) * grain;
        for (std::uint32_t k = 0; k < grain; k += 4) {
            const float x[4] = {samples[k], samples[k + 1], samples[k + 2], samples[k + 3]};
            float y[4];
            for (int j = 0; j < 4; j++) {
                const auto weight = [&](int m) { return m <= j ? filter.h[j - m] : 0.0f; };
                float a = weight(2) * x[2];
                a = std::fma(filter.e2[j], h.x2, a);
                a = std::fma(weight(0), x[0], a);
                a = std::fma(filter.d[j], h.y2, a);
                float b = weight(3) * x[3];
                b = std::fma(filter.e1[j], h.x1, b);
                b = std::fma(weight(1), x[1], b);
                b = std::fma(filter.c[j], h.y1, b);
                y[j] = a + b;
            }
            h = {x[3], x[2], y[3], y[2]};
            std::copy(y, y + 4, samples + k);
        }
    }
}
