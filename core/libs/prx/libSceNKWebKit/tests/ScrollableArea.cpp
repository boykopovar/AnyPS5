#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

constexpr int kUnit = 64;

WebCore::LayoutUnit Px(int px) {
    return WebCore::LayoutUnit{px * kUnit};
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ScrollableArea check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    const WebCore::LayoutRect viewport{{Px(0), Px(0)}, {Px(800), Px(600)}};
    const WebCore::LayoutSize contents{Px(1600), Px(2000)};
    const WebCore::LayoutPoint zeroOrigin{Px(0), Px(0)};

    auto inBounds = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(250), Px(400)}, zeroOrigin, 0, 0);
    Require(inBounds.m_x.m_value == Px(250).m_value && inBounds.m_y.m_value == Px(400).m_value);

    auto negativeOverhang = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(-50), Px(-120)}, zeroOrigin, 0, 0);
    Require(negativeOverhang.m_x.m_value == 0 && negativeOverhang.m_y.m_value == 0);

    auto maxOverhang = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(950), Px(1700)}, zeroOrigin, 0, 0);
    Require(maxOverhang.m_x.m_value == Px(800).m_value && maxOverhang.m_y.m_value == Px(1400).m_value);

    auto headerFooter = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(100), Px(1800)}, zeroOrigin, 50, 100);
    Require(headerFooter.m_x.m_value == Px(100).m_value && headerFooter.m_y.m_value == Px(1250).m_value);

    auto aboveHeader = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(0), Px(-10)}, zeroOrigin, 50, 100);
    Require(aboveHeader.m_x.m_value == 0 && aboveHeader.m_y.m_value == 0);

    const WebCore::LayoutPoint rtlOrigin{Px(800), Px(0)};
    auto rtlClampedLeft = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(-900), Px(100)}, rtlOrigin, 0, 0);
    Require(rtlClampedLeft.m_x.m_value == Px(-800).m_value && rtlClampedLeft.m_y.m_value == Px(100).m_value);

    auto rtlClampedRight = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(200), Px(100)}, rtlOrigin, 0, 0);
    Require(rtlClampedRight.m_x.m_value == 0 && rtlClampedRight.m_y.m_value == Px(100).m_value);

    const WebCore::LayoutSize smallContents{Px(300), Px(200)};
    auto smallDoc = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, smallContents, {Px(100), Px(100)}, zeroOrigin, 0, 0);
    Require(smallDoc.m_x.m_value == 0 && smallDoc.m_y.m_value == 0);

    auto fractional = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport,
        {WebCore::LayoutUnit{1000 * kUnit + 32}, WebCore::LayoutUnit{800 * kUnit + 16}},
        {WebCore::LayoutUnit{500 * kUnit}, WebCore::LayoutUnit{300 * kUnit}},
        zeroOrigin,
        0,
        0);
    Require(fractional.m_x.m_value == 200 * kUnit + 32 && fractional.m_y.m_value == 200 * kUnit + 16);

    auto saturatedHeader = _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
        viewport, contents, {Px(0), Px(0)}, zeroOrigin, std::numeric_limits<int>::max(), 0);
    Require(saturatedHeader.m_x.m_value == Px(-800).m_value && saturatedHeader.m_y.m_value == Px(-600).m_value);

    return 0;
}
