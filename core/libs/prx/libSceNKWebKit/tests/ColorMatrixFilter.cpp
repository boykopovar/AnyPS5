#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ColorMatrixFilterExports.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::ColorMatrixFilterOperationAbi;
using NKWebKit::LegacyFilterOperationType;
using Color = WebCore::SRGBA<float>;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "color matrix check failed at line %u\n", location.line());
        std::abort();
    }
}

ColorMatrixFilterOperationAbi Operation(LegacyFilterOperationType type, double amount) {
    return {reinterpret_cast<const void*>(0x1234), 1, type, amount};
}

bool Transform(LegacyFilterOperationType type, double amount, Color& color) {
    const auto operation = Operation(type, amount);
    return _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(&operation, &color);
}

bool Near(float a, float b) {
    return std::fabs(a - b) <= 1e-5f;
}

bool Near(const Color& a, const Color& b) {
    return Near(a.red, b.red) && Near(a.green, b.green) && Near(a.blue, b.blue) && a.alpha == b.alpha;
}

void TestGrayscale() {
    Color full{1.0f, 0.0f, 0.0f, 0.5f};
    Check(Transform(LegacyFilterOperationType::Grayscale, 1.0, full));
    Check(Near(full, {0.2126f, 0.2126f, 0.2126f, 0.5f}));

    Color white{1.0f, 1.0f, 1.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Grayscale, 1.0, white));
    Check(Near(white, {1.0f, 1.0f, 1.0f, 1.0f}));

    Color none{0.25f, 0.5f, 0.75f, 0.25f};
    Check(Transform(LegacyFilterOperationType::Grayscale, 0.0, none));
    Check(Near(none, {0.25f, 0.5f, 0.75f, 0.25f}));

    Color half{0.0f, 1.0f, 0.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Grayscale, 0.5, half));
    Check(Near(half, {0.7152f - 0.7152f * 0.5f, 0.7152f + 0.2848f * 0.5f, 0.7152f - 0.7152f * 0.5f, 1.0f}));
}

void TestGrayscaleClampsAmount() {
    Color over{0.0f, 0.0f, 1.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Grayscale, 3.0, over));
    Check(Near(over, {0.0722f, 0.0722f, 0.0722f, 1.0f}));

    Color under{0.0f, 0.0f, 1.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Grayscale, -2.0, under));
    Check(Near(under, {0.0f, 0.0f, 1.0f, 1.0f}));
}

void TestSepia() {
    Color white{1.0f, 1.0f, 1.0f, 0.75f};
    Check(Transform(LegacyFilterOperationType::Sepia, 1.0, white));
    Check(Near(white, {0.393f + 0.769f + 0.189f, 0.349f + 0.686f + 0.168f, 0.272f + 0.534f + 0.131f, 0.75f}));

    Color none{0.25f, 0.5f, 0.75f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Sepia, 0.0, none));
    Check(Near(none, {0.25f, 0.5f, 0.75f, 1.0f}));
}

void TestSaturateDoesNotClampAmount() {
    Color zero{1.0f, 0.0f, 0.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Saturate, 0.0, zero));
    Check(Near(zero, {0.213f, 0.213f, 0.213f, 1.0f}));

    Color one{0.25f, 0.5f, 0.75f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Saturate, 1.0, one));
    Check(Near(one, {0.25f, 0.5f, 0.75f, 1.0f}));

    Color two{1.0f, 0.0f, 0.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::Saturate, 2.0, two));
    Check(Near(two, {0.213f + 0.787f * 2.0f, 0.213f - 0.213f * 2.0f, 0.213f - 0.213f * 2.0f, 1.0f}));
}

void TestHueRotate() {
    Color zero{0.25f, 0.5f, 0.75f, 1.0f};
    Check(Transform(LegacyFilterOperationType::HueRotate, 0.0, zero));
    Check(Near(zero, {0.25f, 0.5f, 0.75f, 1.0f}));

    Color half{1.0f, 0.0f, 0.0f, 0.5f};
    Check(Transform(LegacyFilterOperationType::HueRotate, 180.0, half));
    Check(Near(half, {0.213f - 0.787f, 0.213f + 0.213f, 0.213f + 0.213f, 0.5f}));

    Color quarter{1.0f, 0.0f, 0.0f, 1.0f};
    Check(Transform(LegacyFilterOperationType::HueRotate, 90.0, quarter));
    Check(Near(quarter, {0.213f - 0.213f, 0.213f + 0.143f, 0.213f - 0.787f, 1.0f}));
}

void TestOtherTypesLeaveColor() {
    const LegacyFilterOperationType others[] = {
        LegacyFilterOperationType::Reference,
        LegacyFilterOperationType::Invert,
        LegacyFilterOperationType::AppleInvertLightness,
        LegacyFilterOperationType::Opacity,
        LegacyFilterOperationType::Brightness,
        LegacyFilterOperationType::Contrast,
        LegacyFilterOperationType::Blur,
        LegacyFilterOperationType::DropShadow,
        LegacyFilterOperationType::Passthrough,
        LegacyFilterOperationType::Default,
        LegacyFilterOperationType::None,
        static_cast<LegacyFilterOperationType>(99),
    };
    for (auto type : others) {
        Color color{0.25f, 0.5f, 0.75f, 1.0f};
        Check(!Transform(type, 2.0, color));
        Check(color.red == 0.25f && color.green == 0.5f && color.blue == 0.75f && color.alpha == 1.0f);
    }
}

void TestNullArgumentsThrow() {
    Color color{};
    const auto operation = Operation(LegacyFilterOperationType::Grayscale, 1.0);
    bool threw = false;
    try {
        _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(nullptr, &color);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
    threw = false;
    try {
        _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(&operation, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
}

}  // namespace

int main() {
    TestGrayscale();
    TestGrayscaleClampsAmount();
    TestSepia();
    TestSaturateDoesNotClampAmount();
    TestHueRotate();
    TestOtherTypesLeaveColor();
    TestNullArgumentsThrow();
    return 0;
}
