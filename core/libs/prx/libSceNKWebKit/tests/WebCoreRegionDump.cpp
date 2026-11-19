#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionDumpExports.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
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
        std::fprintf(stderr, "Region dump check failed at line %u\n", location.line());
        std::abort();
    }
}

struct RegionHolder {
    alignas(alignof(WebCore::Region)) char storage[sizeof(WebCore::Region)];
    WebCore::Region* ptr() { return reinterpret_cast<WebCore::Region*>(storage); }
};

TextStream NewStream(bool multiLine, unsigned flags = 0, int indent = 0) {
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

std::string Dump(WebCore::Region& region, bool multiLine = true, unsigned flags = 0, int indent = 0) {
    TextStream stream = NewStream(multiLine, flags, indent);
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(stream, region);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

WebCore::IntRect Rect(int x, int y, int width, int height) {
    return WebCore::IntRect{WebCore::IntPoint{x, y}, WebCore::IntSize{width, height}};
}

void TestEmptyRegion() {
    RegionHolder region;
    _ZN7WebCore6RegionC1Ev(region.ptr());
    Check(Dump(*region.ptr()) == "\n");
    _ZN7WebCore6RegionD1Ev(region.ptr());
}

void TestSingleRect() {
    RegionHolder region;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(region.ptr(), Rect(10, 20, 30, 40));
    Check(Dump(*region.ptr()) == "\n  (rect (10,20) width=30 height=40)\n");
    Check(Dump(*region.ptr(), true, 0, 2) == "\n      (rect (10,20) width=30 height=40)\n");
    Check(Dump(*region.ptr(), false) == "\n(rect (10,20) width=30 height=40)\n");
    Check(Dump(*region.ptr(), true, 1u << 0) == "\n  (rect at (10,20) size 30x40)\n");
    _ZN7WebCore6RegionD1Ev(region.ptr());
}

void TestShapeRegion() {
    RegionHolder region;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(region.ptr(), Rect(0, 0, 10, 10));
    RegionHolder other;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(other.ptr(), Rect(20, 0, 5, 10));
    _ZN7WebCore6Region5uniteERKS0_(region.ptr(), *other.ptr());
    Check(Dump(*region.ptr()) ==
          "\n"
          "  (rect (0,0) width=10 height=10)\n"
          "  (rect (20,0) width=5 height=10)\n");
    _ZN7WebCore6RegionD1Ev(other.ptr());
    _ZN7WebCore6RegionD1Ev(region.ptr());
}

void TestNonRectangularShape() {
    RegionHolder region;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(region.ptr(), Rect(0, 0, 10, 10));
    RegionHolder other;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(other.ptr(), Rect(5, 5, 10, 10));
    _ZN7WebCore6Region5uniteERKS0_(region.ptr(), *other.ptr());
    Check(Dump(*region.ptr()) ==
          "\n"
          "  (rect (0,0) width=10 height=5)\n"
          "  (rect (0,5) width=15 height=5)\n"
          "  (rect (5,10) width=10 height=5)\n");
    _ZN7WebCore6RegionD1Ev(other.ptr());
    _ZN7WebCore6RegionD1Ev(region.ptr());
}

}  // namespace

int main() {
    TestEmptyRegion();
    TestSingleRect();
    TestShapeRegion();
    TestNonRectangularShape();
    return 0;
}
