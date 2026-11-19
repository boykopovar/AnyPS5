#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ComponentTransferFilterExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::ComponentTransferFilterOperationAbi;
using NKWebKit::LegacyFilterOperationType;
using Color = WebCore::SRGBA<float>;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "component transfer check failed at line %u\n", location.line());
        std::abort();
    }
}

ComponentTransferFilterOperationAbi Operation(LegacyFilterOperationType type, double amount) {
    return {reinterpret_cast<const void*>(0x1234), 1, type, amount};
}

bool Transform(const ComponentTransferFilterOperationAbi& operation, Color& color) {
    return _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(&operation, &color);
}

bool Same(const Color& a, const Color& b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

void TestOpacityScalesAlphaOnly() {
    Color color{0.25f, 0.5f, 0.75f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Opacity, 0.5), color));
    Check(Same(color, {0.25f, 0.5f, 0.75f, 0.25f}));
}

void TestOpacityDoesNotClamp() {
    Color color{0, 0, 0, 0.75f};
    Check(Transform(Operation(LegacyFilterOperationType::Opacity, 2.0), color));
    Check(color.alpha == 1.5f);
}

void TestInvert() {
    Color full{0.25f, 0.5f, 1.0f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Invert, 1.0), full));
    Check(Same(full, {0.75f, 0.5f, 0.0f, 0.5f}));

    Color half{0.25f, 0.5f, 1.0f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Invert, 0.5), half));
    Check(Same(half, {0.5f, 0.5f, 0.5f, 0.5f}));

    Color none{0.25f, 0.5f, 1.0f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Invert, 0.0), none));
    Check(Same(none, {0.25f, 0.5f, 1.0f, 0.5f}));
}

void TestInvertComputesInDouble() {
    Color color{0.1f, 0, 0, 1};
    Check(Transform(Operation(LegacyFilterOperationType::Invert, 0.3), color));
    const float oneMinusAmount = static_cast<float>(1.0f - 0.3);
    const float expected = static_cast<float>(1.0f - (oneMinusAmount + 0.1f * (0.3 - oneMinusAmount)));
    Check(color.red == expected);
}

void TestContrastClamps() {
    Color color{0.5f, 0.75f, 0.125f, 0.25f};
    Check(Transform(Operation(LegacyFilterOperationType::Contrast, 2.0), color));
    Check(Same(color, {0.5f, 1.0f, 0.0f, 0.25f}));

    Color flat{0.0f, 0.5f, 1.0f, 1.0f};
    Check(Transform(Operation(LegacyFilterOperationType::Contrast, 0.0), flat));
    Check(Same(flat, {0.5f, 0.5f, 0.5f, 1.0f}));
}

void TestBrightnessClampsOnlyBelowZero() {
    Color color{0.25f, 0.75f, 0.0f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Brightness, 2.0), color));
    Check(Same(color, {0.5f, 1.5f, 0.0f, 0.5f}));

    Color negative{0.25f, 0.75f, 1.0f, 0.5f};
    Check(Transform(Operation(LegacyFilterOperationType::Brightness, -1.0), negative));
    Check(Same(negative, {0.0f, 0.0f, 0.0f, 0.5f}));
}

void TestOtherTypesLeaveColor() {
    const LegacyFilterOperationType others[] = {
        LegacyFilterOperationType::Reference,
        LegacyFilterOperationType::Grayscale,
        LegacyFilterOperationType::Sepia,
        LegacyFilterOperationType::Saturate,
        LegacyFilterOperationType::HueRotate,
        LegacyFilterOperationType::AppleInvertLightness,
        LegacyFilterOperationType::Blur,
        LegacyFilterOperationType::DropShadow,
        LegacyFilterOperationType::Passthrough,
        LegacyFilterOperationType::Default,
        LegacyFilterOperationType::None,
        static_cast<LegacyFilterOperationType>(99),
    };
    for (auto type : others) {
        Color color{0.25f, 0.5f, 0.75f, 1.0f};
        Check(!Transform(Operation(type, 2.0), color));
        Check(Same(color, {0.25f, 0.5f, 0.75f, 1.0f}));
    }
}

void TestNullArgumentsThrow() {
    Color color{};
    auto operation = Operation(LegacyFilterOperationType::Opacity, 1.0);
    bool threw = false;
    try {
        _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(nullptr, &color);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
    threw = false;
    try {
        _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(&operation, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
}

}  // namespace

int main() {
    TestOpacityScalesAlphaOnly();
    TestOpacityDoesNotClamp();
    TestInvert();
    TestInvertComputesInDouble();
    TestContrastClamps();
    TestBrightnessClampsOnlyBelowZero();
    TestOtherTypesLeaveColor();
    TestNullArgumentsThrow();
    return 0;
}
