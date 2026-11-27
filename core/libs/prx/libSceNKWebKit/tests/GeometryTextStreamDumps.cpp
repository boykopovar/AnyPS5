#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WebCoreAbi::FloatBoxExtent;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

constexpr unsigned SVGStyleRect = 1;
constexpr unsigned NumberRespectingIntegers = 2;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GeometryTextStreamDumps check failed at line %u\n", location.line());
        std::abort();
    }
}

TextStream NewStream(unsigned flags, bool multiLine, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = flags;
    stream.multiLineMode = multiLine;
    stream.indent = indent;
    return stream;
}

std::string Release(TextStream& stream) {
    String released = _ZN3WTF10TextStream7releaseEv(&stream);
    std::string result;
    if (!released.impl) return result;
    Check(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

std::string DumpRounded(const WebCore::FloatRoundedRect& value, unsigned flags = 0, bool multiLine = true, int indent = 0) {
    TextStream stream = NewStream(flags, multiLine, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(stream, value);
    Check(&returned == &stream);
    Check(stream.formattingFlags == flags);
    Check(stream.indent == indent);
    return Release(stream);
}

std::string DumpExtent(const FloatBoxExtent& value, unsigned flags = 0, bool multiLine = true) {
    TextStream stream = NewStream(flags, multiLine, 0);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(stream, value);
    Check(&returned == &stream);
    Check(stream.formattingFlags == flags);
    Check(stream.indent == 0);
    return Release(stream);
}

WebCore::FloatRoundedRect Rounded() {
    WebCore::FloatRoundedRect value{};
    value.m_rect = {{1.0f, 2.5f}, {3.0f, 4.0f}};
    value.m_radii.m_topLeft = {1.0f, 2.0f};
    value.m_radii.m_topRight = {3.5f, 4.0f};
    value.m_radii.m_bottomLeft = {0.0f, 0.0f};
    value.m_radii.m_bottomRight = {5.0f, 6.25f};
    return value;
}

void TestFloatRoundedRect() {
    const WebCore::FloatRoundedRect value = Rounded();
    Check(DumpRounded(value) ==
        "(1,2.50) width=3 height=4"
        "\n(top-left width=1 height=2)"
        "\n(top-right width=3.50 height=4)"
        "\n(bottom-left width=0 height=0)"
        "\n(bottom-right width=5 height=6.25)");
    Check(DumpRounded(value, 0, false) ==
        "(1,2.50) width=3 height=4 (top-left width=1 height=2) (top-right width=3.50 height=4)"
        " (bottom-left width=0 height=0) (bottom-right width=5 height=6.25)");
    Check(DumpRounded(value, 0, true, 2) ==
        "(1,2.50) width=3 height=4"
        "\n    (top-left width=1 height=2)"
        "\n    (top-right width=3.50 height=4)"
        "\n    (bottom-left width=0 height=0)"
        "\n    (bottom-right width=5 height=6.25)");
    Check(DumpRounded(value, SVGStyleRect) ==
        "at (1,2.50) size 3x4"
        "\n(top-left width=1 height=2)"
        "\n(top-right width=3.50 height=4)"
        "\n(bottom-left width=0 height=0)"
        "\n(bottom-right width=5 height=6.25)");
    Check(DumpRounded(value, NumberRespectingIntegers, false) ==
        "(1,2.50) width=3 height=4 (top-left width=1 height=2) (top-right width=3.50 height=4)"
        " (bottom-left width=0 height=0) (bottom-right width=5 height=6.25)");

    WebCore::FloatRoundedRect empty{};
    Check(DumpRounded(empty, 0, false) ==
        "(0,0) width=0 height=0 (top-left width=0 height=0) (top-right width=0 height=0)"
        " (bottom-left width=0 height=0) (bottom-right width=0 height=0)");

    WebCore::FloatRoundedRect negative = value;
    negative.m_rect = {{-1.5f, -2.0f}, {10.125f, 0.5f}};
    Check(DumpRounded(negative, 0, false).rfind("(-1.50,-2) width=10.13 height=0.50 (top-left", 0) == 0);
}

void TestFloatBoxExtent() {
    const FloatBoxExtent value{1.0f, 2.5f, -3.25f, 0.0f};
    Check(DumpExtent(value) == "top: 1.00 right: 2.50 bottom: -3.25 left: 0.00");
    Check(DumpExtent(value, 0, false) == "top: 1.00 right: 2.50 bottom: -3.25 left: 0.00");
    Check(DumpExtent(value, SVGStyleRect) == "top: 1.00 right: 2.50 bottom: -3.25 left: 0.00");
    Check(DumpExtent(value, NumberRespectingIntegers) == "top: 1 right: 2.50 bottom: -3.25 left: 0");

    const FloatBoxExtent order{10.0f, 20.0f, 30.0f, 40.0f};
    Check(DumpExtent(order) == "top: 10.00 right: 20.00 bottom: 30.00 left: 40.00");

    const FloatBoxExtent rounding{0.005f, 1.0f / 3.0f, 1234567.0f, -0.0f};
    TextStream reference = NewStream(0, true, 0);
    _ZN3WTF10TextStreamlsEPKc(&reference, "top: ");
    _ZN3WTF10TextStreamlsEf(&reference, rounding.top);
    _ZN3WTF10TextStreamlsEPKc(&reference, " right: ");
    _ZN3WTF10TextStreamlsEf(&reference, rounding.right);
    _ZN3WTF10TextStreamlsEPKc(&reference, " bottom: ");
    _ZN3WTF10TextStreamlsEf(&reference, rounding.bottom);
    _ZN3WTF10TextStreamlsEPKc(&reference, " left: ");
    _ZN3WTF10TextStreamlsEf(&reference, rounding.left);
    Check(DumpExtent(rounding) == Release(reference));
    Check(DumpExtent(rounding).rfind("top: 0.00 right: 0.33 bottom: 1234567.00 left: ", 0) == 0);
}

void TestAppendsToExistingText() {
    TextStream stream = NewStream(0, false, 0);
    _ZN3WTF10TextStreamlsEPKc(&stream, "box ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(stream, FloatBoxExtent{1.0f, 1.0f, 1.0f, 1.0f});
    _ZN3WTF10TextStreamlsEPKc(&stream, "; rect ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(stream, WebCore::FloatRoundedRect{});
    Check(Release(stream) ==
        "box top: 1.00 right: 1.00 bottom: 1.00 left: 1.00; rect (0,0) width=0 height=0 (top-left width=0 height=0)"
        " (top-right width=0 height=0) (bottom-left width=0 height=0) (bottom-right width=0 height=0)");
}

}

int main() {
    TestFloatRoundedRect();
    TestFloatBoxExtent();
    TestAppendsToExistingText();
    return 0;
}
