#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore color space check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

bool Near(float actual, float expected, float tolerance = 1e-4f) {
    return std::fabs(actual - expected) <= tolerance;
}

template<typename TColor> bool NearRgb(const TColor& color, float red, float green, float blue, float alpha) {
    return Near(color.red, red) && Near(color.green, green) && Near(color.blue, blue) && color.alpha == alpha;
}

std::string Text(const NKWebKit::WtfAbi::String& value) {
    Require(value.impl != nullptr);
    Require((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

void TestTransferFunctions() {
    const WebCore::SRGBA<float> srgb{0.5f, 0.04f, 1.0f, 0.25f};
    auto linear = _ZN7WebCore13toLinearSRGBAERKNS_5SRGBAIfEE(srgb);
    Require(NearRgb(linear, 0.214041f, 0.0030960f, 1.0f, 0.25f));
    Require(linear.blue == 1.0f);

    const WebCore::LinearSRGBA<float> linearInput{0.214041f, 0.002f, 2.0f, 0.75f};
    auto encoded = _ZN7WebCore7toSRGBAERKNS_11LinearSRGBAIfEE(linearInput);
    Require(NearRgb(encoded, 0.5f, 0.02584f, 1.0f, 0.75f));
    Require(encoded.blue == 1.0f);

    const WebCore::LinearSRGBA<float> negative{-0.01f, 0.0f, 0.0f, 1.0f};
    auto negativeEncoded = _ZN7WebCore7toSRGBAERKNS_11LinearSRGBAIfEE(negative);
    Require(Near(negativeEncoded.red, -0.1292f));
    Require(negativeEncoded.green == 0.0f);

    const WebCore::DisplayP3<float> p3{0.5f, 0.04f, 1.0f, 0.5f};
    auto linearP3 = _ZN7WebCore17toLinearDisplayP3ERKNS_9DisplayP3IfEE(p3);
    Require(NearRgb(linearP3, 0.214041f, 0.0030960f, 1.0f, 0.5f));

    const WebCore::LinearDisplayP3<float> linearP3Input{0.214041f, 0.002f, 0.0f, 0.5f};
    auto encodedP3 = _ZN7WebCore11toDisplayP3ERKNS_15LinearDisplayP3IfEE(linearP3Input);
    Require(NearRgb(encodedP3, 0.5f, 0.02584f, 0.0f, 0.5f));
}

void TestGamutConversions() {
    auto red = _ZN7WebCore11toDisplayP3ERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{1.0f, 0.0f, 0.0f, 1.0f});
    Require(NearRgb(red, 0.917552f, 0.200304f, 0.138573f, 1.0f));

    auto mixed = _ZN7WebCore11toDisplayP3ERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.2f, 0.4f, 0.6f, 0.5f});
    Require(NearRgb(mixed, 0.249861f, 0.395236f, 0.583972f, 0.5f));

    auto white = _ZN7WebCore11toDisplayP3ERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{1.0f, 1.0f, 1.0f, 1.0f});
    Require(NearRgb(white, 1.0f, 1.0f, 1.0f, 1.0f));

    auto p3Red = _ZN7WebCore7toSRGBAERKNS_9DisplayP3IfEE(WebCore::DisplayP3<float>{1.0f, 0.0f, 0.0f, 0.25f});
    Require(NearRgb(p3Red, 1.0f, -0.543388f, -0.253778f, 0.25f));

    auto p3Mixed = _ZN7WebCore7toSRGBAERKNS_9DisplayP3IfEE(WebCore::DisplayP3<float>{0.3f, 0.5f, 0.7f, 1.0f});
    Require(NearRgb(p3Mixed, 0.225351f, 0.506352f, 0.717869f, 1.0f));
}

void TestHsl() {
    auto red = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{1.0f, 0.0f, 0.0f, 0.5f});
    Require(red.hue == 0.0f && red.saturation == 1.0f && red.lightness == 0.5f && red.alpha == 0.5f);

    auto green = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.0f, 1.0f, 0.0f, 1.0f});
    Require(Near(green.hue, 1.0f / 3.0f) && green.saturation == 1.0f && green.lightness == 0.5f);

    auto jade = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.2f, 0.6f, 0.4f, 1.0f});
    Require(Near(jade.hue, 150.0f / 360.0f) && Near(jade.saturation, 0.5f) && Near(jade.lightness, 0.4f));

    auto blue = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.0f, 0.0f, 1.0f, 1.0f});
    Require(Near(blue.hue, 2.0f / 3.0f));

    auto rose = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{1.0f, 0.0f, 0.5f, 1.0f});
    Require(Near(rose.hue, 330.0f / 360.0f) && rose.saturation == 1.0f && rose.lightness == 0.5f);

    auto tan = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.8f, 0.6f, 0.4f, 1.0f});
    Require(Near(tan.hue, 30.0f / 360.0f) && Near(tan.saturation, 0.5f) && Near(tan.lightness, 0.6f));

    auto dark = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.1f, 0.2f, 0.3f, 1.0f});
    Require(Near(dark.hue, 210.0f / 360.0f) && Near(dark.saturation, 0.5f) && Near(dark.lightness, 0.2f));

    auto gray = _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(WebCore::SRGBA<float>{0.25f, 0.25f, 0.25f, 1.0f});
    Require(gray.hue == 0.0f && gray.saturation == 0.0f && gray.lightness == 0.25f);

    auto fromRed = _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(WebCore::HSLA<float>{0.0f, 1.0f, 0.5f, 0.5f});
    Require(fromRed.red == 1.0f && fromRed.green == 0.0f && fromRed.blue == 0.0f && fromRed.alpha == 0.5f);

    auto fromTan = _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(WebCore::HSLA<float>{30.0f / 360.0f, 0.5f, 0.6f, 1.0f});
    Require(NearRgb(fromTan, 0.8f, 0.6f, 0.4f, 1.0f));

    auto fromDark = _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(WebCore::HSLA<float>{210.0f / 360.0f, 0.5f, 0.2f, 1.0f});
    Require(NearRgb(fromDark, 0.1f, 0.2f, 0.3f, 1.0f));

    auto fromMagenta = _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(WebCore::HSLA<float>{300.0f / 360.0f, 1.0f, 0.5f, 1.0f});
    Require(NearRgb(fromMagenta, 1.0f, 0.0f, 1.0f, 1.0f));

    auto fromGray = _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(WebCore::HSLA<float>{0.7f, 0.0f, 0.3f, 0.2f});
    Require(fromGray.red == 0.3f && fromGray.green == 0.3f && fromGray.blue == 0.3f && fromGray.alpha == 0.2f);
}

void TestByteSerialization() {
    const WebCore::SRGBA<std::uint8_t> opaque{255, 0, 128, 255};
    const WebCore::SRGBA<std::uint8_t> transparent{1, 2, 3, 0};
    const WebCore::SRGBA<std::uint8_t> half{10, 20, 30, 128};
    const WebCore::SRGBA<std::uint8_t> quarter{10, 20, 30, 64};
    const WebCore::SRGBA<std::uint8_t> faint{10, 20, 30, 1};
    const WebCore::SRGBA<std::uint8_t> nearlyOpaque{10, 20, 30, 254};

    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(opaque)) == "rgb(255, 0, 128)");
    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(transparent)) == "rgba(1, 2, 3, 0)");
    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(half)) == "rgba(10, 20, 30, 0.5)");
    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(quarter)) == "rgba(10, 20, 30, 0.25)");
    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(faint)) == "rgba(10, 20, 30, 0.004)");
    Require(Text(_ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(nearlyOpaque)) == "rgba(10, 20, 30, 0.996)");

    Require(Text(_ZN7WebCore20serializationForHTMLENS_5SRGBAIhEE(opaque)) == "#ff0080");
    Require(Text(_ZN7WebCore20serializationForHTMLENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t>{0xAB, 0xCD, 0xEF, 255})) == "#abcdef");
    Require(Text(_ZN7WebCore20serializationForHTMLENS_5SRGBAIhEE(half)) == "rgba(10, 20, 30, 0.5)");

    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextENS_5SRGBAIhEE(opaque)) == "#FF0080");
    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextENS_5SRGBAIhEE(half)) == "#0A141E80");
    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextENS_5SRGBAIhEE(transparent)) == "#01020300");
}

void TestFloatSerialization() {
    const WebCore::SRGBA<float> opaque{1.0f, 0.5f, 0.25f, 1.0f};
    const WebCore::SRGBA<float> translucent{0.1f, 0.0f, 1e-7f, 0.5f};
    Require(Text(_ZN7WebCore19serializationForCSSERKNS_5SRGBAIfEE(opaque)) == "color(srgb 1 0.5 0.25)");
    Require(Text(_ZN7WebCore20serializationForHTMLERKNS_5SRGBAIfEE(opaque)) == "color(srgb 1 0.5 0.25)");
    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextERKNS_5SRGBAIfEE(opaque)) == "color(srgb 1 0.5 0.25)");
    Require(Text(_ZN7WebCore19serializationForCSSERKNS_5SRGBAIfEE(translucent)) == "color(srgb 0.1 0 1e-7 / 0.5)");

    const WebCore::LinearSRGBA<float> linear{0.75f, -0.5f, 2.0f, 0.99999994f};
    Require(Text(_ZN7WebCore19serializationForCSSERKNS_11LinearSRGBAIfEE(linear)) == "color(linear-srgb 0.75 -0.5 2)");
    Require(Text(_ZN7WebCore20serializationForHTMLERKNS_11LinearSRGBAIfEE(linear)) == "color(linear-srgb 0.75 -0.5 2)");
    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextERKNS_11LinearSRGBAIfEE(linear)) == "color(linear-srgb 0.75 -0.5 2)");

    const WebCore::DisplayP3<float> p3{0.3f, 123456.0f, 0.0f, 0.25f};
    Require(Text(_ZN7WebCore19serializationForCSSERKNS_9DisplayP3IfEE(p3)) == "color(display-p3 0.3 123456 0 / 0.25)");
    Require(Text(_ZN7WebCore20serializationForHTMLERKNS_9DisplayP3IfEE(p3)) == "color(display-p3 0.3 123456 0 / 0.25)");
    Require(Text(_ZN7WebCore32serializationForRenderTreeAsTextERKNS_9DisplayP3IfEE(p3)) == "color(display-p3 0.3 123456 0 / 0.25)");

    const WebCore::DisplayP3<float> nearlyOpaque{0.0f, 0.0f, 0.0f, 0.9999f};
    Require(Text(_ZN7WebCore19serializationForCSSERKNS_9DisplayP3IfEE(nearlyOpaque)) == "color(display-p3 0 0 0 / 0.9999)");
}

}  // namespace

int main() {
    TestTransferFunctions();
    TestGamutConversions();
    TestHsl();
    TestByteSerialization();
    TestFloatSerialization();
    std::puts("WebCore color space tests passed");
    return 0;
}
