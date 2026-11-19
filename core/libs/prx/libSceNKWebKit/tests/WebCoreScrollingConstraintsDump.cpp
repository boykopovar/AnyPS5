#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ScrollingConstraintsDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ScrollingConstraints dump check failed at line %u\n", location.line());
        std::abort();
    }
}

TextStream NewStream(bool multiLine, unsigned flags, int indent) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.multiLineMode = multiLine;
    stream.formattingFlags = flags;
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

template <typename T, auto Write>
std::string Dump(const T& constraints, bool multiLine, unsigned flags = 0, int indent = 0) {
    TextStream stream = NewStream(multiLine, flags, indent);
    TextStream& returned = Write(stream, constraints);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

std::string DumpAbsolute(const WebCore::AbsolutePositionConstraints& constraints, bool multiLine, unsigned flags = 0, int indent = 0) {
    return Dump<WebCore::AbsolutePositionConstraints, _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE>(constraints, multiLine, flags, indent);
}

std::string DumpFixed(const WebCore::FixedPositionViewportConstraints& constraints, bool multiLine, unsigned flags = 0, int indent = 0) {
    return Dump<WebCore::FixedPositionViewportConstraints, _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE>(constraints, multiLine, flags, indent);
}

std::string DumpSticky(const WebCore::StickyPositionViewportConstraints& constraints, bool multiLine, unsigned flags = 0, int indent = 0) {
    return Dump<WebCore::StickyPositionViewportConstraints, _ZN7WebCorelsERN3WTF10TextStreamERKNS_33StickyPositionViewportConstraintsE>(constraints, multiLine, flags, indent);
}

WebCore::FloatRect Rect(float x, float y, float width, float height) {
    return WebCore::FloatRect{WebCore::FloatPoint{x, y}, WebCore::FloatSize{width, height}};
}

void TestAbsolute() {
    WebCore::AbsolutePositionConstraints constraints{WebCore::FloatSize{9, 9}, WebCore::FloatPoint{3, -4}};
    Check(DumpAbsolute(constraints, true) == "\n(layer-position-at-last-layout (3,-4))");
    Check(DumpAbsolute(constraints, false) == " (layer-position-at-last-layout (3,-4))");
    Check(DumpAbsolute(constraints, true, 0, 2) == "\n    (layer-position-at-last-layout (3,-4))");
}

void TestFixed() {
    WebCore::FixedPositionViewportConstraints constraints{nullptr, WebCore::FloatSize{7, 7}, 5, Rect(0, 10, 800, 600), WebCore::FloatPoint{12, 34}};
    Check(DumpFixed(constraints, true) ==
          "\n(viewport-rect-at-last-layout (0,10) width=800 height=600)"
          "\n(layer-position-at-last-layout (12,34))");
    Check(DumpFixed(constraints, false) ==
          " (viewport-rect-at-last-layout (0,10) width=800 height=600)"
          " (layer-position-at-last-layout (12,34))");
}

void TestSticky() {
    WebCore::StickyPositionViewportConstraints constraints{
        nullptr, WebCore::FloatSize{1, 1}, 4, 1, 2, 3, 4,
        Rect(0, 0, 1000, 700), Rect(5, 6, 300, 400), Rect(10, 20, 50, 60),
        WebCore::FloatSize{0, 15}, WebCore::FloatPoint{10, 35}};
    Check(DumpSticky(constraints, true, 0, 1) ==
          "\n  (sticky-position-at-last-layout width=0 height=15)"
          "\n  (layer-position-at-last-layout (10,35))"
          "\n  (sticky-box-rect (10,20) width=50 height=60)"
          "\n  (containing-block-rect (5,6) width=300 height=400)"
          "\n  (constraining-rect-at-last-layout (0,0) width=1000 height=700)");
}

void TestFractionsAndSvgStyle() {
    WebCore::StickyPositionViewportConstraints constraints{
        nullptr, WebCore::FloatSize{}, 0, 0, 0, 0, 0,
        Rect(0, 0, 1, 1), Rect(0, 0, 2, 2), Rect(1.5f, 2, 3, 4.25f),
        WebCore::FloatSize{0.5f, 0}, WebCore::FloatPoint{-1.75f, 2}};
    Check(DumpSticky(constraints, false, 1u << 0) ==
          " (sticky-position-at-last-layout width=0.50 height=0)"
          " (layer-position-at-last-layout (-1.75,2))"
          " (sticky-box-rect at (1.50,2) size 3x4.25)"
          " (containing-block-rect at (0,0) size 2x2)"
          " (constraining-rect-at-last-layout at (0,0) size 1x1)");
}

}  // namespace

int main() {
    TestAbsolute();
    TestFixed();
    TestSticky();
    TestFractionsAndSvgStyle();
    return 0;
}
