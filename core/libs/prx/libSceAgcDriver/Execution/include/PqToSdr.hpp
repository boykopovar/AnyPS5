#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PQTOSDR_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PQTOSDR_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "prx/libSceAgcDriver/Execution/include/ToneMapLuminance.hpp"

namespace AgcDriver {

inline constexpr float PqReferenceWhiteNits = 203.0f;
inline constexpr float PqToneMapKnee = 0.75f;
inline constexpr std::uint32_t PqCodes = 1024;

inline const std::array<float, PqCodes>& PqEotfNits() {
    static const auto table = [] {
        std::array<float, PqCodes> nits{};
        constexpr double m1 = 2610.0 / 16384.0;
        constexpr double m2 = 2523.0 / 4096.0 * 128.0;
        constexpr double c1 = 3424.0 / 4096.0;
        constexpr double c2 = 2413.0 / 4096.0 * 32.0;
        constexpr double c3 = 2392.0 / 4096.0 * 32.0;
        for (std::uint32_t code = 0; code < PqCodes; ++code) {
            const double e = std::pow(code / 1023.0, 1.0 / m2);
            nits[code] = static_cast<float>(10000.0 * std::pow(std::max(e - c1, 0.0) / (c2 - c3 * e), 1.0 / m1));
        }
        return nits;
    }();
    return table;
}

inline float PqReferenceWhite(float whiteNits) {
    return std::min(PqReferenceWhiteNits, whiteNits);
}

inline float PqToneMap(float luminance, float relativeWhite) {
    if (luminance <= PqToneMapKnee) return luminance;
    const float t = (luminance - PqToneMapKnee) / (1.0f - PqToneMapKnee);
    const float w = (relativeWhite - PqToneMapKnee) / (1.0f - PqToneMapKnee);
    return PqToneMapKnee + (1.0f - PqToneMapKnee) * t * (1.0f + t / (w * w)) / (1.0f + t);
}

inline std::array<float, 3> PqToSdrLinear(float red, float green, float blue, float whiteNits) {
    const float referenceWhite = PqReferenceWhite(whiteNits);
    const float r2020 = red / referenceWhite;
    const float g2020 = green / referenceWhite;
    const float b2020 = blue / referenceWhite;
    std::array<float, 3> rgb{
        std::max(1.6604910f * r2020 - 0.5876411f * g2020 - 0.0728499f * b2020, 0.0f),
        std::max(-0.1245505f * r2020 + 1.1328999f * g2020 - 0.0083494f * b2020, 0.0f),
        std::max(-0.0181508f * r2020 - 0.1005789f * g2020 + 1.1187297f * b2020, 0.0f)};
    const float luminance = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
    if (luminance > 0.0f) {
        const float scale = PqToneMap(luminance, whiteNits / referenceWhite) / luminance;
        for (auto& channel : rgb) channel = std::min(channel * scale, 1.0f);
    }
    return rgb;
}

inline std::uint8_t SrgbEncode8(float linear) {
    const float v = std::clamp(linear, 0.0f, 1.0f);
    const float encoded = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::floor(encoded * 255.0f + 0.5f));
}

inline std::array<std::uint8_t, 4> PqToSdrPixel(std::uint32_t word, bool redLow, float whiteNits) {
    const auto& nits = PqEotfNits();
    const float low = nits[word & 0x3ffu];
    const float green = nits[(word >> 10u) & 0x3ffu];
    const float high = nits[(word >> 20u) & 0x3ffu];
    const auto rgb = PqToSdrLinear(redLow ? low : high, green, redLow ? high : low, whiteNits);
    return {SrgbEncode8(rgb[2]), SrgbEncode8(rgb[1]), SrgbEncode8(rgb[0]), 0xff};
}

}

#endif
