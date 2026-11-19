#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/FloatRoundedRectDumpExports.hpp"
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
        std::fprintf(stderr, "FloatRoundedRect dump check failed at line %u\n", location.line());
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

std::string Dump(const WebCore::FloatRoundedRect& roundedRect, bool multiLine = true, unsigned flags = 0, int indent = 0) {
    TextStream stream = NewStream(multiLine, flags, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(stream, roundedRect);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

WebCore::FloatRoundedRect Rounded(float x, float y, float width, float height, WebCore::FloatSize topLeft, WebCore::FloatSize topRight,
                                  WebCore::FloatSize bottomLeft, WebCore::FloatSize bottomRight) {
    return WebCore::FloatRoundedRect{
        WebCore::FloatRect{WebCore::FloatPoint{x, y}, WebCore::FloatSize{width, height}},
        WebCore::FloatRoundedRect::Radii{topLeft, topRight, bottomLeft, bottomRight}};
}

void TestMultiLine() {
    auto roundedRect = Rounded(1, 2, 30, 40, {1, 2}, {3, 4}, {5, 6}, {7, 8});
    Check(Dump(roundedRect) ==
          "(1,2) width=30 height=40\n"
          "(top-left width=1 height=2)\n"
          "(top-right width=3 height=4)\n"
          "(bottom-left width=5 height=6)\n"
          "(bottom-right width=7 height=8)");
}

void TestIndent() {
    auto roundedRect = Rounded(0, 0, 10, 10, {0, 0}, {0, 0}, {0, 0}, {0, 0});
    Check(Dump(roundedRect, true, 0, 1) ==
          "(0,0) width=10 height=10\n"
          "  (top-left width=0 height=0)\n"
          "  (top-right width=0 height=0)\n"
          "  (bottom-left width=0 height=0)\n"
          "  (bottom-right width=0 height=0)");
}

void TestSingleLine() {
    auto roundedRect = Rounded(1, 2, 30, 40, {1, 2}, {3, 4}, {5, 6}, {7, 8});
    Check(Dump(roundedRect, false) ==
          "(1,2) width=30 height=40 (top-left width=1 height=2) (top-right width=3 height=4)"
          " (bottom-left width=5 height=6) (bottom-right width=7 height=8)");
}

void TestFractionsAndSvgStyle() {
    auto roundedRect = Rounded(1.5f, -2, 3.25f, 4, {0.5f, 1}, {2, 2}, {3, 3}, {4, 4.75f});
    Check(Dump(roundedRect, false, 1u << 0) ==
          "at (1.50,-2) size 3.25x4 (top-left width=0.50 height=1) (top-right width=2 height=2)"
          " (bottom-left width=3 height=3) (bottom-right width=4 height=4.75)");
}

}  // namespace

int main() {
    TestMultiLine();
    TestIndent();
    TestSingleLine();
    TestFractionsAndSvgStyle();
    return 0;
}
