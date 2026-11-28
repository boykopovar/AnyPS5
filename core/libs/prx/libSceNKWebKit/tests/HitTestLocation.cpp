#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

using NKWebKit::WebCoreAbi::LegacyHitTestLocation;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "hit test location check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

static const auto constructors = {&_ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE, &_ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE};

struct Expected {
    float x;
    float y;
    int rawX;
    int rawY;
    int boxX;
    int boxY;
    float pointX;
    float pointY;
    float left;
    float top;
    float right;
    float bottom;
};

static void RequireLocation(const Expected& expected, int line) {
    for (const auto construct : constructors) {
        alignas(4) unsigned char bytes[68];
        std::memset(bytes, 0xA5, sizeof(bytes));
        auto* location = reinterpret_cast<LegacyHitTestLocation*>(bytes);
        const WebCore::FloatPoint point{expected.x, expected.y};
        construct(location, &point);
        Check(location->point.m_x.m_value == expected.rawX && location->point.m_y.m_value == expected.rawY, line);
        Check(location->boundingBox.m_location.m_x == expected.boxX && location->boundingBox.m_location.m_y == expected.boxY, line);
        Check(location->boundingBox.m_size.m_width == 1 && location->boundingBox.m_size.m_height == 1, line);
        Check(location->transformedPoint.m_x == expected.pointX && location->transformedPoint.m_y == expected.pointY, line);
        const WebCore::FloatQuad& quad = location->transformedRect;
        Check(quad.m_p1.m_x == expected.left && quad.m_p1.m_y == expected.top, line);
        Check(quad.m_p2.m_x == expected.right && quad.m_p2.m_y == expected.top, line);
        Check(quad.m_p3.m_x == expected.right && quad.m_p3.m_y == expected.bottom, line);
        Check(quad.m_p4.m_x == expected.left && quad.m_p4.m_y == expected.bottom, line);
        Check(bytes[64] == 0 && bytes[65] == 1, line);
    }
}

#define Location(...) RequireLocation(Expected{__VA_ARGS__}, __LINE__)

int main() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Location(0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Location(10.5f, 20.25f, 672, 1296, 10, 20, 10.5f, 20.25f, 10.0f, 20.0f, 11.0f, 21.0f);
    Location(3.0f, 7.0f, 192, 448, 3, 7, 3.0f, 7.0f, 3.0f, 7.0f, 4.0f, 8.0f);
    Location(-0.5f, -1.75f, -32, -112, -1, -2, -0.5f, -1.75f, -1.0f, -2.0f, 0.0f, -1.0f);
    Location(-3.0f, -64.0f, -192, -4096, -3, -64, -3.0f, -64.0f, -3.0f, -64.0f, -2.0f, -63.0f);
    Location(0.0078125f, 0.99f, 0, 63, 0, 0, 0.0f, 0.984375f, 0.0f, 0.0f, 1.0f, 1.0f);
    Location(-0.0078125f, -0.001f, -1, -1, -1, -1, -0.015625f, -0.015625f, -1.0f, -1.0f, 0.0f, 0.0f);
    Location(1.999f, -1.0001f, 127, -65, 1, -2, 1.984375f, -1.015625f, 1.0f, -2.0f, 2.0f, -1.0f);
    Location(1.0e10f, -1.0e10f, INT_MAX, INT_MIN, 33554431, -33554432, 33554432.0f, -33554432.0f, 33554432.0f, -33554432.0f, 33554432.0f, -33554432.0f);
    Location(infinity, -infinity, INT_MAX, INT_MIN, 33554431, -33554432, 33554432.0f, -33554432.0f, 33554432.0f, -33554432.0f, 33554432.0f, -33554432.0f);
    Location(nan, 5.0f, INT_MIN, 320, -33554432, 5, -33554432.0f, 5.0f, -33554432.0f, 5.0f, -33554432.0f, 6.0f);
    Location(33554430.0f, -33554430.0f, 2147483520, -2147483520, 33554430, -33554430, 33554430.0f, -33554430.0f, 33554430.0f, -33554430.0f, 33554432.0f, -33554428.0f);
    for (const auto construct : constructors) {
        LegacyHitTestLocation location{};
        const WebCore::FloatPoint point{1.0f, 2.0f};
        bool thrown = false;
        try {
            construct(nullptr, &point);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }
        Require(thrown);
        thrown = false;
        try {
            construct(&location, nullptr);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }
        Require(thrown && location.boundingBox.m_size.m_width == 0);
    }
    std::puts("libSceNKWebKit hit test location tests passed");
    return 0;
}
