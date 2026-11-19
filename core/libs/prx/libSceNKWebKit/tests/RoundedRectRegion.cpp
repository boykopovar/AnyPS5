#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RoundedRectRegionExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "RoundedRectRegion check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static WebCore::LayoutUnit Raw(int value) {
    return WebCore::LayoutUnit{value};
}

static WebCore::LayoutUnit Pixels(int value) {
    return WebCore::LayoutUnit{value * 64};
}

static WebCore::LayoutSize Size(int width, int height) {
    return WebCore::LayoutSize{Pixels(width), Pixels(height)};
}

static WebCore::RoundedRect MakeRoundedRect(int x, int y, int width, int height, WebCore::LayoutSize topLeft, WebCore::LayoutSize topRight,
    WebCore::LayoutSize bottomLeft, WebCore::LayoutSize bottomRight) {
    WebCore::RoundedRect rect;
    rect.m_rect = WebCore::LayoutRect{WebCore::LayoutPoint{Pixels(x), Pixels(y)}, WebCore::LayoutSize{Pixels(width), Pixels(height)}};
    rect.m_radii = WebCore::RoundedRect::Radii{topLeft, topRight, bottomLeft, bottomRight};
    return rect;
}

static bool Contains(const WebCore::Region& region, int x, int y) {
    WebCore::IntPoint point{x, y};
    return _ZNK7WebCore6Region8containsERKNS_8IntPointE(&region, point);
}

static std::uint64_t Area(const WebCore::Region& region) {
    return _ZNK7WebCore6Region9totalAreaEv(&region);
}

static bool SameRect(const WebCore::IntRect& rect, int x, int y, int width, int height) {
    return rect.m_location.m_x == x && rect.m_location.m_y == y && rect.m_size.m_width == width && rect.m_size.m_height == height;
}

static void TestEmptyRect() {
    WebCore::RoundedRect rect = MakeRoundedRect(5, 5, 0, 10, Size(2, 2), Size(2, 2), Size(2, 2), Size(2, 2));
    WebCore::Region region = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 0);
    Require(region.isEmpty());
    Require(region.isRect());
    Require(Area(region) == 0);

    rect.m_rect.m_size.m_width = Raw(-64);
    rect.m_rect.m_size.m_height = Pixels(10);
    WebCore::Region negative = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 4);
    Require(negative.isEmpty());
}

static void TestEnclosingRect() {
    WebCore::RoundedRect rect;
    rect.m_rect = WebCore::LayoutRect{WebCore::LayoutPoint{Raw(672), Raw(1296)}, WebCore::LayoutSize{Raw(1920), Raw(2560)}};
    WebCore::Region region = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 0);
    Require(region.isRect());
    Require(SameRect(region.bounds(), 10, 20, 31, 41));
    Require(Area(region) == 31u * 41u);

    rect.m_rect = WebCore::LayoutRect{WebCore::LayoutPoint{Raw(-100), Raw(-64)}, WebCore::LayoutSize{Raw(64), Raw(1)}};
    WebCore::Region negative = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 3);
    Require(SameRect(negative.bounds(), -2, -1, 2, 1));
}

static void TestOneStepPerCorner() {
    WebCore::RoundedRect rect = MakeRoundedRect(0, 0, 100, 100, Size(10, 10), Size(10, 10), Size(10, 10), Size(10, 10));
    WebCore::Region region = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 10);
    Require(!region.isRect());
    Require(SameRect(region.bounds(), 0, 0, 100, 100));
    Require(Area(region) == 10000u - 4u * 9u);
    Require(!Contains(region, 0, 0));
    Require(!Contains(region, 2, 2));
    Require(Contains(region, 3, 3));
    Require(!Contains(region, 99, 0));
    Require(!Contains(region, 97, 2));
    Require(Contains(region, 96, 2));
    Require(!Contains(region, 0, 99));
    Require(!Contains(region, 99, 99));
    Require(!Contains(region, 97, 97));
    Require(Contains(region, 96, 97));
    Require(Contains(region, 50, 0));
    Require(Contains(region, 0, 50));
    Require(Contains(region, 99, 50));
    Require(Contains(region, 50, 99));
}

static void TestTwoStepsInOneCorner() {
    WebCore::RoundedRect rect = MakeRoundedRect(0, 0, 100, 100, Size(0, 0), Size(0, 0), Size(0, 0), Size(20, 20));
    WebCore::Region region = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 10);
    Require(Area(region) == 10000u - 54u);
    Require(Contains(region, 0, 0));
    Require(Contains(region, 99, 0));
    Require(Contains(region, 0, 99));
    Require(Contains(region, 96, 96));
    Require(Contains(region, 89, 99));
    Require(Contains(region, 99, 88));
    Require(!Contains(region, 99, 89));
    Require(!Contains(region, 97, 89));
    Require(Contains(region, 96, 89));
    Require(!Contains(region, 97, 90));
    Require(!Contains(region, 90, 97));
    Require(!Contains(region, 99, 99));
    Require(!Contains(region, 97, 99));
    Require(Contains(region, 96, 90));
    Require(Contains(region, 90, 96));
}

static void TestStepLengthRounding() {
    WebCore::RoundedRect rect = MakeRoundedRect(0, 0, 100, 100, Size(10, 10), Size(10, 10), Size(10, 10), Size(10, 10));
    WebCore::Region coarse = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 21);
    Require(coarse.isRect());
    Require(Area(coarse) == 10000u);

    WebCore::Region boundary = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rect, 20);
    Require(Area(boundary) == 10000u - 4u * 9u);

    WebCore::RoundedRect elliptical = MakeRoundedRect(0, 0, 100, 100, Size(0, 0), Size(0, 0), Size(0, 0), Size(40, 10));
    WebCore::Region fromMinimum = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(elliptical, 21);
    Require(fromMinimum.isRect());
}

static void TestZeroStepLength() {
    WebCore::RoundedRect square = MakeRoundedRect(1, 2, 3, 4, Size(0, 0), Size(0, 0), Size(0, 0), Size(0, 0));
    WebCore::Region region = _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(square, 0);
    Require(SameRect(region.bounds(), 1, 2, 3, 4));

    WebCore::RoundedRect rounded = MakeRoundedRect(1, 2, 3, 4, Size(0, 0), Size(0, 0), Size(0, 0), Size(1, 1));
    bool threw = false;
    try {
        static_cast<void>(_ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(rounded, 0));
    } catch (const std::domain_error&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestEmptyRect();
    TestEnclosingRect();
    TestOneStepPerCorner();
    TestTwoStepsInOneCorner();
    TestStepLengthRounding();
    TestZeroStepLength();
    std::puts("RoundedRectRegion tests passed");
    return 0;
}
