#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FrameView viewport math check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int L(int pixels) {
    return pixels * 64;
}

static WebCore::LayoutPoint Point(int x, int y) {
    return WebCore::LayoutPoint{WebCore::LayoutUnit{x}, WebCore::LayoutUnit{y}};
}

static WebCore::LayoutSize Size(int width, int height) {
    return WebCore::LayoutSize{WebCore::LayoutUnit{width}, WebCore::LayoutUnit{height}};
}

static WebCore::LayoutRect Rect(int x, int y, int width, int height) {
    return WebCore::LayoutRect{Point(x, y), Size(width, height)};
}

static bool Same(const WebCore::LayoutPoint& point, int x, int y) {
    return point.m_x.m_value == x && point.m_y.m_value == y;
}

static bool Same(const WebCore::LayoutSize& size, int width, int height) {
    return size.m_width.m_value == width && size.m_height.m_value == height;
}

static bool Same(const WebCore::LayoutRect& rect, int x, int y, int width, int height) {
    return Same(rect.m_location, x, y) && Same(rect.m_size, width, height);
}

static constexpr auto StickToDocument = WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds;
static constexpr auto StickToViewport = WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds;

static WebCore::LayoutSize Expanded(const WebCore::LayoutSize& base, const WebCore::LayoutSize& document, double factor) {
    return _ZN7WebCore9FrameView26expandedLayoutViewportSizeERKNS_10LayoutSizeES3_d(base, document, factor);
}

static WebCore::LayoutPoint Origin(const WebCore::LayoutRect& visual, const WebCore::LayoutPoint& stableMin,
    const WebCore::LayoutPoint& stableMax, const WebCore::LayoutRect& layout, WebCore::ScrollBehaviorForFixedElements behavior) {
    return _ZN7WebCore9FrameView27computeLayoutViewportOriginERKNS_10LayoutRectERKNS_11LayoutPointES6_S3_NS_30ScrollBehaviorForFixedElementsE(
        visual, stableMin, stableMax, layout, behavior);
}

static WebCore::LayoutRect Updated(const WebCore::LayoutRect& document, WebCore::LayoutViewportConstraint constraint) {
    return _ZN7WebCore9FrameView32computeUpdatedLayoutViewportRectERKNS_10LayoutRectES3_RKNS_10LayoutSizeES3_S6_RKNS_11LayoutPointES9_NS0_24LayoutViewportConstraintE(
        Rect(L(5), L(5), L(100), L(100)), document, Size(L(300), L(500)), Rect(L(950), L(-30), L(300), L(500)),
        Size(L(320), L(480)), Point(0, 0), Point(L(680), L(320)), constraint);
}

static WebCore::LayoutPoint Fixed(const WebCore::LayoutRect& visible, const WebCore::LayoutPoint& scrollPosition, float scale,
    bool relativeToFrame, WebCore::ScrollBehaviorForFixedElements behavior, int header, int footer) {
    return _ZN7WebCore9FrameView30scrollPositionForFixedPositionERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_fbNS_30ScrollBehaviorForFixedElementsEii(
        visible, Size(L(1000), L(2000)), scrollPosition, Point(0, 0), scale, relativeToFrame, behavior, header, footer);
}

static WebCore::FloatPoint RootContent(float scrollY, float inset) {
    return _ZN7WebCore9FrameView27positionForRootContentLayerERKNS_10FloatPointES3_ff(
        WebCore::FloatPoint{10.0f, scrollY}, WebCore::FloatPoint{4.0f, 6.0f}, inset, 12.0f);
}

static void TestExpandedLayoutViewportSize() {
    Require(Same(Expanded(Size(L(320), L(480)), Size(L(320), L(2000)), 0.0), L(320), L(480)));
    Require(Same(Expanded(Size(L(320), L(480)), Size(L(320), L(400)), 0.5), L(320), L(480)));
    Require(Same(Expanded(Size(L(320), L(480)), Size(L(320), L(600)), 0.5), L(320), L(600)));
    Require(Same(Expanded(Size(L(320), L(480)), Size(L(320), L(2000)), 0.5), L(320), L(720)));
    Require(Same(Expanded(Size(L(320), 1001), Size(L(320), L(2000)), 0.25), L(320), 1251));
}

static void TestComputeLayoutViewportOrigin() {
    WebCore::LayoutRect wideVisual = Rect(L(-10), L(50), L(400), L(300));
    WebCore::LayoutRect layout = Rect(0, 0, L(320), L(480));
    Require(Same(Origin(wideVisual, Point(0, 0), Point(L(100), L(500)), layout, StickToDocument), 0, 0));
    Require(Same(Origin(wideVisual, Point(0, 0), Point(L(100), L(500)), layout, StickToViewport), L(-10), 0));

    WebCore::LayoutRect leftVisual = Rect(L(-20), L(100), L(200), L(300));
    WebCore::LayoutRect shiftedLayout = Rect(L(10), 0, L(320), L(480));
    Require(Same(Origin(leftVisual, Point(0, 0), Point(L(500), L(500)), shiftedLayout, StickToViewport), L(-20), 0));
    Require(Same(Origin(leftVisual, Point(0, 0), Point(L(500), L(500)), shiftedLayout, StickToDocument), 0, 0));

    WebCore::LayoutRect rightVisual = Rect(L(700), 0, L(200), L(300));
    WebCore::LayoutRect rightLayout = Rect(L(100), 0, L(320), L(480));
    Require(Same(Origin(rightVisual, Point(0, 0), Point(L(500), L(500)), rightLayout, StickToViewport), L(580), 0));
    Require(Same(Origin(rightVisual, Point(0, 0), Point(L(500), L(500)), rightLayout, StickToDocument), L(500), 0));

    WebCore::LayoutRect tallVisual = Rect(0, L(900), L(100), L(600));
    Require(Same(Origin(tallVisual, Point(0, 0), Point(L(500), L(500)), layout, StickToDocument), 0, L(500)));
    Require(Same(Origin(tallVisual, Point(0, 0), Point(L(500), L(500)), layout, StickToViewport), 0, L(900)));
}

static void TestComputeUpdatedLayoutViewportRect() {
    WebCore::LayoutRect document = Rect(0, 0, L(1000), L(800));
    Require(Same(Updated(document, WebCore::LayoutViewportConstraint::Unconstrained), L(930), L(-30), L(320), L(500)));
    Require(Same(Updated(document, WebCore::LayoutViewportConstraint::ConstrainedToDocumentRect), L(680), 0, L(320), L(500)));
    WebCore::LayoutRect narrowDocument = Rect(0, 0, L(200), L(800));
    Require(Same(Updated(narrowDocument, WebCore::LayoutViewportConstraint::ConstrainedToDocumentRect), L(-120), 0, L(320), L(500)));
}

static void TestScrollPositionForFixedPosition() {
    WebCore::LayoutRect visible = Rect(0, 0, L(400), L(300));
    Require(Same(Fixed(visible, Point(L(-50), L(-40)), 1.0f, true, StickToDocument, 20, 10), 0, 0));
    Require(Same(Fixed(visible, Point(L(700), L(1800)), 1.0f, true, StickToDocument, 20, 10), L(600), L(1670)));
    Require(Same(Fixed(visible, Point(L(-50), L(-40)), 1.0f, true, StickToViewport, 20, 10), L(-50), L(-60)));
    Require(Same(Fixed(visible, Point(L(300), L(500)), 2.0f, true, StickToViewport, 0, 0), L(150), L(250)));
    Require(Same(Fixed(visible, Point(L(300), L(500)), 2.0f, false, StickToViewport, 0, 0), L(50), 13176));
    WebCore::LayoutRect fullWidth = Rect(0, 0, L(1000), L(300));
    Require(Same(Fixed(fullWidth, Point(L(10), L(500)), 2.0f, false, StickToViewport, 0, 0), L(5), 13176));
}

static void TestPositionForRootContentLayer() {
    WebCore::FloatPoint noInset = RootContent(30.0f, 0.0f);
    Require(noInset.m_x == -4.0f && noInset.m_y == 6.0f);
    WebCore::FloatPoint insetBelowScroll = RootContent(30.0f, 50.0f);
    Require(insetBelowScroll.m_x == -4.0f && insetBelowScroll.m_y == 36.0f);
    WebCore::FloatPoint insetAboveScroll = RootContent(30.0f, 20.0f);
    Require(insetAboveScroll.m_x == -4.0f && insetAboveScroll.m_y == 26.0f);
    WebCore::FloatPoint negativeScroll = RootContent(-15.0f, 50.0f);
    Require(negativeScroll.m_x == -4.0f && negativeScroll.m_y == 6.0f);
}

int main() {
    TestExpandedLayoutViewportSize();
    TestComputeLayoutViewportOrigin();
    TestComputeUpdatedLayoutViewportRect();
    TestScrollPositionForFixedPosition();
    TestPositionForRootContentLayer();
    std::printf("FrameView viewport math tests passed\n");
    return 0;
}
