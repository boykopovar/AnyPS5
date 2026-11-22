#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "WebCore static helper check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

void RequireRect(const WebCore::IntRect& rect, int x, int y, int width, int height, int line) {
    Check(rect.m_location.m_x == x, line);
    Check(rect.m_location.m_y == y, line);
    Check(rect.m_size.m_width == width, line);
    Check(rect.m_size.m_height == height, line);
}
#define RequireRectEq(rect, x, y, w, h) RequireRect((rect), (x), (y), (w), (h), __LINE__)

WebCore::IntRect RectFor(int rawX, int rawY, unsigned top, unsigned right, unsigned bottom, unsigned left) {
    WebCore::LayoutPoint point{{rawX}, {rawY}};
    return _ZN7WebCore15HitTestLocation12rectForPointERKNS_11LayoutPointEjjjj(point, top, right, bottom, left);
}

void TestIsoBoxStatics() {
    Require(_ZN7WebCore12ISOWebVTTCue11boxTypeNameEv().value == 0x76747463u);
    Require(_ZN7WebCore6ISOBox14minimumBoxSizeEv() == 8u);
}

void TestDefaultTextEncodingName() {
    NKWebKit::WtfAbi::String name = _ZN7WebCore40defaultTextEncodingNameForSystemLanguageEv();
    Require(name.impl != nullptr);
    Require((name.impl->hashAndFlags & 4) != 0);
    Require(name.impl->length == 10);
    Require(std::memcmp(name.impl->data, "ISO-8859-1", 10) == 0);
    NKWebKit::WtfAbi::String second = _ZN7WebCore40defaultTextEncodingNameForSystemLanguageEv();
    Require(second.impl != nullptr && second.impl->length == 10);
    Require(std::memcmp(second.impl->data, "ISO-8859-1", 10) == 0);
}

void TestRectForPoint() {
    RequireRectEq(RectFor(64, 128, 0, 0, 0, 0), 1, 2, 1, 1);
    RequireRectEq(RectFor(64, 128, 1, 2, 3, 4), -3, 1, 7, 5);
    RequireRectEq(RectFor(63, 127, 0, 0, 0, 0), 0, 1, 1, 1);
    RequireRectEq(RectFor(-1, -64, 0, 0, 0, 0), -1, -1, 1, 1);
    RequireRectEq(RectFor(-65, -128, 0, 0, 0, 0), -2, -2, 1, 1);
    RequireRectEq(RectFor(INT_MIN + 63, INT_MIN, 0, 0, 0, 0), INT_MIN / 64, INT_MIN / 64, 1, 1);
    RequireRectEq(RectFor(INT_MIN + 64, INT_MAX, 0, 0, 0, 0), INT_MIN / 64 + 1, INT_MAX / 64, 1, 1);
    RequireRectEq(RectFor(0, 0, 0, 0, 0, UINT_MAX), 1, 0, 0, 1);
    RequireRectEq(RectFor(0, 0, 5, 0, 7, 0), 0, -5, 1, 13);
}

}  // namespace

int main() {
    TestIsoBoxStatics();
    TestDefaultTextEncodingName();
    TestRectForPoint();
    return 0;
}
