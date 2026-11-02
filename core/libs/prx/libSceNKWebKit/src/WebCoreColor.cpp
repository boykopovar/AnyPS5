#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace WebCore {

namespace {

constexpr std::uint64_t kFlagsShift = 48;
constexpr std::uint64_t kColorSpaceShift = 56;
constexpr std::uint64_t kOutOfLineFlag = 1ULL << (kFlagsShift + 3);
constexpr std::uint64_t kValidFlag = 1ULL << (kFlagsShift + 2);
constexpr std::uint64_t kColorSpaceSRGB = 17ULL << kColorSpaceShift;

bool IsOutOfLine(const Color& c) {
    return (c.m_colorAndFlags & kOutOfLineFlag) != 0;
}

std::uint8_t Red(const Color& c) {
    return static_cast<std::uint8_t>((c.m_colorAndFlags >> 24) & 0xFF);
}

std::uint8_t Green(const Color& c) {
    return static_cast<std::uint8_t>((c.m_colorAndFlags >> 16) & 0xFF);
}

std::uint8_t Blue(const Color& c) {
    return static_cast<std::uint8_t>((c.m_colorAndFlags >> 8) & 0xFF);
}

std::uint8_t Alpha(const Color& c) {
    return static_cast<std::uint8_t>(c.m_colorAndFlags & 0xFF);
}

Color MakeSRGBA(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, std::uint64_t extraFlags = 0) {
    std::uint64_t packed = (static_cast<std::uint64_t>(r) << 24)
                         | (static_cast<std::uint64_t>(g) << 16)
                         | (static_cast<std::uint64_t>(b) << 8)
                         | static_cast<std::uint64_t>(a);
    return Color{packed | kColorSpaceSRGB | kValidFlag | extraFlags};
}

}  // namespace
}  // namespace WebCore

extern "C" {
void APS5_VABI _ZN7WebCore5ColorC1EOS0_(WebCore::Color* self, WebCore::Color&& other) {
    if (WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
    other.m_colorAndFlags = 0;
}

void APS5_VABI _ZN7WebCore5ColorC2EOS0_(WebCore::Color* self, WebCore::Color&& other) {
    if (WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
    other.m_colorAndFlags = 0;
}

void APS5_VABI _ZN7WebCore5ColorC1ERKS0_(WebCore::Color* self, const WebCore::Color& other) {
    if (WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
}

void APS5_VABI _ZN7WebCore5ColorC2ERKS0_(WebCore::Color* self, const WebCore::Color& other) {
    if (WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
}

WebCore::Color& APS5_VABI _ZN7WebCore5ColoraSEOS0_(WebCore::Color* self, WebCore::Color&& other) {
    if (WebCore::IsOutOfLine(*self) || WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
    other.m_colorAndFlags = 0;
    return *self;
}

WebCore::Color& APS5_VABI _ZN7WebCore5ColoraSERKS0_(WebCore::Color* self, const WebCore::Color& other) {
    if (WebCore::IsOutOfLine(*self) || WebCore::IsOutOfLine(other)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    self->m_colorAndFlags = other.m_colorAndFlags;
    return *self;
}

WebCore::Color APS5_VABI _ZNK7WebCore5Color14colorWithAlphaEf(const WebCore::Color* self, float alpha) {
    if (WebCore::IsOutOfLine(*self)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    std::uint8_t a = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(alpha * 255.0f)), 0l, 255l));
    std::uint64_t semanticFlag = self->m_colorAndFlags & (1ULL << WebCore::kFlagsShift);
    return WebCore::MakeSRGBA(WebCore::Red(*self), WebCore::Green(*self), WebCore::Blue(*self), a, semanticFlag);
}

WebCore::Color APS5_VABI _ZNK7WebCore5Color8darkenedEv(const WebCore::Color* self) {
    if (WebCore::IsOutOfLine(*self)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    std::uint8_t r = WebCore::Red(*self);
    std::uint8_t g = WebCore::Green(*self);
    std::uint8_t b = WebCore::Blue(*self);
    std::uint8_t a = WebCore::Alpha(*self);
    if (r == 255 && g == 255 && b == 255 && a == 255) {
        return WebCore::MakeSRGBA(171, 171, 171, 255);
    }
    float rf = static_cast<float>(r) / 255.0f;
    float gf = static_cast<float>(g) / 255.0f;
    float bf = static_cast<float>(b) / 255.0f;
    float v = std::max({rf, gf, bf});
    float multiplier = (v > 0.0f) ? std::max(0.0f, (v - 0.33f) / v) : 0.0f;
    std::uint8_t nr = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * rf * 255.0f)), 0l, 255l));
    std::uint8_t ng = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * gf * 255.0f)), 0l, 255l));
    std::uint8_t nb = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * bf * 255.0f)), 0l, 255l));
    return WebCore::MakeSRGBA(nr, ng, nb, a);
}

WebCore::Color APS5_VABI _ZNK7WebCore5Color9lightenedEv(const WebCore::Color* self) {
    if (WebCore::IsOutOfLine(*self)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    std::uint8_t r = WebCore::Red(*self);
    std::uint8_t g = WebCore::Green(*self);
    std::uint8_t b = WebCore::Blue(*self);
    std::uint8_t a = WebCore::Alpha(*self);
    if (r == 0 && g == 0 && b == 0 && a == 255) {
        return WebCore::MakeSRGBA(84, 84, 84, 255);
    }
    float rf = static_cast<float>(r) / 255.0f;
    float gf = static_cast<float>(g) / 255.0f;
    float bf = static_cast<float>(b) / 255.0f;
    float v = std::max({rf, gf, bf});
    if (v == 0.0f) {
        return WebCore::MakeSRGBA(84, 84, 84, a);
    }
    float multiplier = std::min(1.0f, v + 0.33f) / v;
    std::uint8_t nr = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * rf * 255.0f)), 0l, 255l));
    std::uint8_t ng = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * gf * 255.0f)), 0l, 255l));
    std::uint8_t nb = static_cast<std::uint8_t>(std::clamp(static_cast<long>(std::lround(multiplier * bf * 255.0f)), 0l, 255l));
    return WebCore::MakeSRGBA(nr, ng, nb, a);
}

double APS5_VABI _ZNK7WebCore5Color9lightnessEv(const WebCore::Color* self) {
    if (WebCore::IsOutOfLine(*self)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    float rf = static_cast<float>(WebCore::Red(*self)) / 255.0f;
    float gf = static_cast<float>(WebCore::Green(*self)) / 255.0f;
    float bf = static_cast<float>(WebCore::Blue(*self)) / 255.0f;
    auto [minVal, maxVal] = std::minmax({rf, gf, bf});
    return 0.5 * static_cast<double>(maxVal + minVal);
}

double APS5_VABI _ZNK7WebCore5Color9luminanceEv(const WebCore::Color* self) {
    if (WebCore::IsOutOfLine(*self)) {
        throw std::invalid_argument("OutOfLine Color unsupported");
    }
    float rf = static_cast<float>(WebCore::Red(*self)) / 255.0f;
    float gf = static_cast<float>(WebCore::Green(*self)) / 255.0f;
    float bf = static_cast<float>(WebCore::Blue(*self)) / 255.0f;
    auto toLinear = [](float c) -> float {
        if (c <= 0.04045f) {
            return std::max(c / 12.92f, 0.0f);
        }
        return std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    float rLin = toLinear(rf);
    float gLin = toLinear(gf);
    float bLin = toLinear(bf);
    return 0.21263900587151027f * rLin + 0.715168678767756f * gLin + 0.07219231536073371f * bLin;
}

}
