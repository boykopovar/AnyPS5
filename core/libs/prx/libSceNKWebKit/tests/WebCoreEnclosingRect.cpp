#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore enclosing rect check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int kIntMax = std::numeric_limits<int>::max();
static constexpr int kIntMin = std::numeric_limits<int>::min();

static bool IntRectIs(const WebCore::IntRect& r, int x, int y, int w, int h) {
    return r.m_location.m_x == x && r.m_location.m_y == y && r.m_size.m_width == w && r.m_size.m_height == h;
}

static bool LayoutRectIs(const WebCore::LayoutRect& r, int x, int y, int w, int h) {
    return r.m_location.m_x.m_value == x && r.m_location.m_y.m_value == y
        && r.m_size.m_width.m_value == w && r.m_size.m_height.m_value == h;
}

static bool FloatRectIs(const WebCore::FloatRect& r, float x, float y, float w, float h) {
    return r.x() == x && r.y() == y && r.width() == w && r.height() == h;
}

static WebCore::FloatRect MakeFloatRect(float x, float y, float w, float h) {
    return WebCore::FloatRect{{x, y}, {w, h}};
}

static WebCore::LayoutRect MakeLayoutRect(int x, int y, int w, int h) {
    return WebCore::LayoutRect{{{x}, {y}}, {{w}, {h}}};
}

static void TestEnclosingIntRectFromFloatRect() {
    auto enclose = [](float x, float y, float w, float h) {
        return _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(MakeFloatRect(x, y, w, h));
    };
    Require(IntRectIs(enclose(1.5f, -2.25f, 3.0f, 0.5f), 1, -3, 4, 2));
    Require(IntRectIs(enclose(2.0f, 3.0f, 4.0f, 5.0f), 2, 3, 4, 5));
    Require(IntRectIs(enclose(0.5f, 0.5f, 0.0f, 0.0f), 0, 0, 1, 1));
    Require(IntRectIs(enclose(-1e10f, 0.0f, 3e10f, 1.0f), kIntMin, 0, kIntMax, 1));
}

static void TestEnclosingIntRectFromLayoutRect() {
    auto enclose = [](int x, int y, int w, int h) {
        return _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(MakeLayoutRect(x, y, w, h));
    };
    Require(IntRectIs(enclose(96, -32, 64, 0), 1, -1, 2, 0));
    Require(IntRectIs(enclose(64, 128, 192, 256), 1, 2, 3, 4));
    Require(IntRectIs(enclose(-96, -1, 1, 1), -2, -1, 1, 1));
    Require(IntRectIs(enclose(32, 32, 0, 0), 0, 0, 0, 0));
    Require(IntRectIs(enclose(kIntMax - 10, 0, 100, 64), 33554431, 0, 0, 1));
    Require(IntRectIs(enclose(kIntMin, 0, 64, 64), -33554432, 0, 1, 1));
}

static void TestEnclosingLayoutRect() {
    auto enclose = [](float x, float y, float w, float h) {
        return _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(MakeFloatRect(x, y, w, h));
    };
    Require(LayoutRectIs(enclose(0.01f, -0.01f, 1.0f, 1.0f), 0, -1, 65, 65));
    Require(LayoutRectIs(enclose(1.5f, 2.0f, 0.25f, 0.0f), 96, 128, 16, 0));
    Require(LayoutRectIs(enclose(-1e9f, 0.0f, 2e9f, 0.0f), kIntMin, 0, kIntMax, 0));
}

static void TestEncloseRectToDevicePixels() {
    auto enclose = [](float x, float y, float w, float h, float scale) {
        return _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(MakeFloatRect(x, y, w, h), scale);
    };
    Require(FloatRectIs(enclose(0.3f, 0.3f, 1.0f, 1.0f, 2.0f), 0.0f, 0.0f, 1.5f, 1.5f));
    Require(FloatRectIs(enclose(0.3f, 0.3f, 1.0f, 1.0f, 1.0f), 0.0f, 0.0f, 2.0f, 2.0f));
    Require(FloatRectIs(enclose(-0.3f, 1.25f, 1.0f, 0.5f, 2.0f), -0.5f, 1.0f, 1.5f, 1.0f));
}

int main() {
    TestEnclosingIntRectFromFloatRect();
    TestEnclosingIntRectFromLayoutRect();
    TestEnclosingLayoutRect();
    TestEncloseRectToDevicePixels();
    return 0;
}
