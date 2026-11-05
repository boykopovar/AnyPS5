#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_HALFFLOATSCANOUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_HALFFLOATSCANOUT_HPP

#include <array>
#include <cmath>
#include <cstdint>

namespace AgcDriver::Graphics {

inline constexpr std::uint32_t HalfFloatScanoutCodes = 255;
inline constexpr std::uint32_t HalfFloatPositiveInfinity = 0x7c00u;

inline const std::array<std::uint32_t, 2 * HalfFloatScanoutCodes>& HalfFloatScanoutThresholds() {
    static const auto thresholds = [] {
        std::array<std::uint32_t, 2 * HalfFloatScanoutCodes> table{};
        const auto finiteValue = [](std::uint32_t bits) {
            const auto exponent = static_cast<int>(bits >> 10u);
            const auto mantissa = static_cast<double>(bits & 0x3ffu);
            return exponent == 0 ? std::ldexp(mantissa, -24) : std::ldexp(1024.0 + mantissa, exponent - 25);
        };
        const auto firstAtLeast = [&](double value) {
            std::uint32_t low = 0;
            std::uint32_t high = HalfFloatPositiveInfinity;
            while (low < high) {
                const auto middle = (low + high) / 2u;
                if (finiteValue(middle) >= value) high = middle;
                else low = middle + 1u;
            }
            return low;
        };
        for (std::uint32_t code = 1; code <= HalfFloatScanoutCodes; ++code) {
            const double encoded = (code - 0.5) / 255.0;
            table[code - 1u] = firstAtLeast(encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4));
            table[HalfFloatScanoutCodes + code - 1u] = firstAtLeast(encoded);
        }
        return table;
    }();
    return thresholds;
}

inline std::uint8_t HalfFloatScanoutCode(std::uint16_t bits, bool alpha) {
    static std::array<std::array<std::uint8_t, 65536>, 2> codes{};
    static const bool filled = [] {
        const auto& thresholds = HalfFloatScanoutThresholds();
        for (std::uint32_t kind = 0; kind < 2u; ++kind) {
            for (std::uint32_t half = 0; half <= HalfFloatPositiveInfinity; ++half) {
                std::uint32_t code = 0;
                while (code < HalfFloatScanoutCodes && thresholds[kind * HalfFloatScanoutCodes + code] <= half) ++code;
                codes[kind][half] = static_cast<std::uint8_t>(code);
            }
        }
        return true;
    }();
    static_cast<void>(filled);
    return codes[alpha ? 1u : 0u][bits];
}

}

#endif
