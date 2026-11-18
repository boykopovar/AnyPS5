#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "RoundedRect check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static WebCore::LayoutRect MakeRect(int x, int y, int width, int height) {
    WebCore::LayoutRect rect{};
    rect.m_location.m_x.m_value = x;
    rect.m_location.m_y.m_value = y;
    rect.m_size.m_width.m_value = width;
    rect.m_size.m_height.m_value = height;
    return rect;
}

static WebCore::RoundedRect::Radii MakeRadii(int seed) {
    WebCore::RoundedRect::Radii radii{};
    radii.m_topLeft.m_width.m_value = seed + 1;
    radii.m_topLeft.m_height.m_value = seed + 2;
    radii.m_topRight.m_width.m_value = seed + 3;
    radii.m_topRight.m_height.m_value = seed + 4;
    radii.m_bottomLeft.m_width.m_value = seed + 5;
    radii.m_bottomLeft.m_height.m_value = seed + 6;
    radii.m_bottomRight.m_width.m_value = seed + 7;
    radii.m_bottomRight.m_height.m_value = seed + 8;
    return radii;
}

static void CheckEqual(const WebCore::RoundedRect& actual, const WebCore::LayoutRect& rect, const WebCore::RoundedRect::Radii& radii) {
    Require(actual.m_rect.m_location.m_x.m_value == rect.m_location.m_x.m_value);
    Require(actual.m_rect.m_location.m_y.m_value == rect.m_location.m_y.m_value);
    Require(actual.m_rect.m_size.m_width.m_value == rect.m_size.m_width.m_value);
    Require(actual.m_rect.m_size.m_height.m_value == rect.m_size.m_height.m_value);
    Require(actual.m_radii.m_topLeft.m_width.m_value == radii.m_topLeft.m_width.m_value);
    Require(actual.m_radii.m_topLeft.m_height.m_value == radii.m_topLeft.m_height.m_value);
    Require(actual.m_radii.m_topRight.m_width.m_value == radii.m_topRight.m_width.m_value);
    Require(actual.m_radii.m_topRight.m_height.m_value == radii.m_topRight.m_height.m_value);
    Require(actual.m_radii.m_bottomLeft.m_width.m_value == radii.m_bottomLeft.m_width.m_value);
    Require(actual.m_radii.m_bottomLeft.m_height.m_value == radii.m_bottomLeft.m_height.m_value);
    Require(actual.m_radii.m_bottomRight.m_width.m_value == radii.m_bottomRight.m_width.m_value);
    Require(actual.m_radii.m_bottomRight.m_height.m_value == radii.m_bottomRight.m_height.m_value);
}

int main() {
    const auto rect = MakeRect(-11, 17, 83, 29);
    const auto radii = MakeRadii(40);

    WebCore::RoundedRect c1{};
    _ZN7WebCore11RoundedRectC1ERKNS_10LayoutRectERKNS0_5RadiiE(&c1, rect, radii);
    CheckEqual(c1, rect, radii);

    const auto secondRect = MakeRect(101, -203, 307, 409);
    const auto secondRadii = MakeRadii(-20);
    WebCore::RoundedRect c2{};
    _ZN7WebCore11RoundedRectC2ERKNS_10LayoutRectERKNS0_5RadiiE(&c2, secondRect, secondRadii);
    CheckEqual(c2, secondRect, secondRadii);
    return 0;
}
