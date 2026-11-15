#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

constexpr unsigned SVGStyleRect = 1;
constexpr unsigned NumberRespectingIntegers = 2;
constexpr unsigned LayoutUnitsAsIntegers = 4;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore dump check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename T> void Throws(T action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw, location);
}

TextStream NewStream(unsigned flags = 0, bool multiLine = true) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    TextStream stream{builder};
    stream.formattingFlags = flags;
    stream.multiLineMode = multiLine;
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

template<typename Function> std::string Dump(Function function, unsigned flags = 0, bool multiLine = true) {
    TextStream stream = NewStream(flags, multiLine);
    TextStream& returned = function(stream);
    Check(&returned == &stream);
    Check(stream.formattingFlags == flags);
    return Release(stream);
}

void TestIntegerGeometry() {
    WebCore::IntPoint point{3, -4};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(s, point); }) == "(3,-4)");
    WebCore::IntSize size{5, 6};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(s, size); }) == "width=5 height=6");
    WebCore::IntRect rect{{1, 2}, {3, 4}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(s, rect); };
    Check(Dump(dump) == "(1,2) width=3 height=4");
    Check(Dump(dump, SVGStyleRect) == "at (1,2) size 3x4");
    Check(Dump(dump, NumberRespectingIntegers) == "(1,2) width=3 height=4");
}

void TestFloatGeometry() {
    auto point = [](WebCore::FloatPoint value, unsigned flags = 0) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(s, value); }, flags);
    };
    Check(point({1.0f, 2.5f}) == "(1,2.50)");
    Check(point({-1.5f, 2.0f}) == "(-1.50,2)");
    Check(point({0.00001f, 3.0f}) == "(0,3)");
    Check(point({1.0f, 2.5f}, SVGStyleRect | NumberRespectingIntegers) == "(1,2.50)");

    auto size = [](WebCore::FloatSize value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(s, value); });
    };
    Check(size({3.0f, 0.25f}) == "width=3 height=0.25");

    WebCore::FloatRect rect{{1.0f, 2.5f}, {3.0f, 4.0f}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(s, rect); };
    Check(Dump(dump) == "(1,2.50) width=3 height=4");
    Check(Dump(dump, SVGStyleRect) == "at (1,2.50) size 3x4");

    WebCore::FloatPoint3D point3{1.0f, 2.5f, -3.0f};
    auto dump3 = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_12FloatPoint3DE(s, point3); };
    Check(Dump(dump3) == "1.00 2.50 -3.00");
    Check(Dump(dump3, NumberRespectingIntegers) == "1 2.50 -3");
}

void TestEnums() {
    auto composite = [](WebCore::CompositeOperator value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17CompositeOperatorE(s, value); });
    };
    Check(composite(WebCore::CompositeOperator::Clear) == "clear");
    Check(composite(WebCore::CompositeOperator::SourceOver) == "source-over");
    Check(composite(WebCore::CompositeOperator::XOR) == "xor");
    Check(composite(WebCore::CompositeOperator::PlusDarker) == "darker");
    Check(composite(WebCore::CompositeOperator::PlusLighter) == "lighter");
    Check(composite(WebCore::CompositeOperator::Difference) == "difference");
    Throws([&] { composite(static_cast<WebCore::CompositeOperator>(14)); });

    auto blend = [](WebCore::BlendMode value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_9BlendModeE(s, value); });
    };
    Check(blend(WebCore::BlendMode::Normal) == "normal");
    Check(blend(WebCore::BlendMode::ColorDodge) == "color-dodge");
    Check(blend(WebCore::BlendMode::Difference) == "difference");
    Check(blend(WebCore::BlendMode::PlusLighter) == "plus-lighter");
    Throws([&] { blend(static_cast<WebCore::BlendMode>(0)); });
    Throws([&] { blend(static_cast<WebCore::BlendMode>(19)); });

    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(s, WebCore::WindRule::NonZero); }) == "NON-ZERO");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(s, WebCore::WindRule::EvenOdd); }) == "EVEN-ODD");

    const char* caps[] = {"BUTT", "ROUND", "SQUARE"};
    for (int i = 0; i < 3; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(s, static_cast<WebCore::LineCap>(i)); }) == caps[i]);
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(s, static_cast<WebCore::LineCap>(3)); }).empty());

    const char* joins[] = {"MITER", "ROUND", "BEVEL"};
    for (int i = 0; i < 3; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8LineJoinE(s, static_cast<WebCore::LineJoin>(i)); }) == joins[i]);

    const char* spaces[] = {"A98-RGB", "DisplayP3", "Extended A98-RGB", "Extended DisplayP3", "Extended Linear sRGB", "Extended ProPhotoRGB",
                            "Extended Rec2020", "Extended sRGB", "HSL", "HWB", "LCH", "Lab", "Linear sRGB", "OKLCH", "OKLab",
                            "ProPhotoRGB", "Rec2020", "sRGB", "XYZ-D50", "XYZ-D50"};
    for (int i = 0; i < 20; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(s, static_cast<WebCore::ColorSpace>(i)); }) == spaces[i]);
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(s, static_cast<WebCore::ColorSpace>(20)); }).empty());

    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(s, WebCore::AlphaPremultiplication::Premultiplied); }) == "premultiplied");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(s, WebCore::AlphaPremultiplication::Unpremultiplied); }) == "unpremultiplied");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(s, WebCore::ScrollType::User); }) == "user");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(s, WebCore::ScrollType::Programmatic); }) == "programmatic");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(s, WebCore::ScrollClamping::Unclamped); }) == "unclamped");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(s, WebCore::ScrollClamping::Clamped); }) == "clamped");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(s, WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds); }) == "0");
    Check(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(s, WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds); }) == "1");
}

void TestScrollAlignment() {
    using Behavior = WebCore::ScrollAlignment::Behavior;
    const char* names[] = {"NoScroll", "AlignCenter", "AlignTop", "AlignBottom", "AlignLeft", "AlignRight", "AlignToClosestEdge"};
    for (int i = 0; i < 7; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(s, static_cast<Behavior>(i)); }) == names[i]);
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(s, static_cast<Behavior>(99)); }).empty());

    WebCore::ScrollAlignment alignment{Behavior::NoScroll, Behavior::AlignCenter, Behavior::AlignToClosestEdge};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15ScrollAlignmentE(s, alignment); })
        == "ScrollAlignment: visible: NoScroll hidden: AlignCenter partial: AlignToClosestEdge");
}

void TestLayoutValues() {
    auto unit = [](int raw, unsigned flags = 0) {
        WebCore::LayoutUnit value{raw};
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutUnitE(s, value); }, flags);
    };
    Check(unit(96) == "1.50");
    Check(unit(128) == "2");
    Check(unit(-96) == "-1.50");
    Check(unit(0) == "0");

    WebCore::LayoutSize size{{96}, {128}};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutSizeE(s, size); }) == "width=1.50 height=2.00");

    WebCore::LayoutPoint point{{96}, {-96}};
    auto dumpPoint = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_11LayoutPointE(s, point); };
    Check(Dump(dumpPoint) == "(1.50,-1.50)");
    Check(Dump(dumpPoint, LayoutUnitsAsIntegers) == "(1,-1)");

    WebCore::LayoutRect rect{{{96}, {160}}, {{192}, {64}}};
    auto dumpRect = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(s, rect); };
    Check(Dump(dumpRect) == "(1.50,2.50) width=3 height=1");
    Check(Dump(dumpRect, SVGStyleRect) == "at (1.50,2.50) size 3x1");
    Check(Dump(dumpRect, LayoutUnitsAsIntegers) == "(2,3) width=3 height=1");
    Check(Dump(dumpRect, LayoutUnitsAsIntegers | SVGStyleRect) == "at (2,3) size 3x1");

    WebCore::LayoutRect negative{{{-96}, {0}}, {{64}, {64}}};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(s, negative); }, LayoutUnitsAsIntegers) == "(-1,0) width=1 height=1");
}

void TestTransforms() {
    WebCore::AffineTransform identity;
    auto dumpIdentity = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(s, identity); };
    Check(Dump(dumpIdentity) == "identity");

    WebCore::AffineTransform transform;
    transform.m_transform = {2.0, 0.0, 0.0, 3.0, 10.5, -1.0};
    auto dumpTransform = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(s, transform); };
    Check(Dump(dumpTransform) == "{m=((2.00,0.00)(0.00,3.00)) t=(10.50,-1.00)}");
    Check(Dump(dumpTransform, NumberRespectingIntegers) == "{m=((2,0)(0,3)) t=(10.50,-1)}");
    transform.m_transform = {1.0, 0.0, 0.0, 1.0, 0.0, 0.5};
    Check(Dump(dumpTransform) == "{m=((1.00,0.00)(0.00,1.00)) t=(0.00,0.50)}");

    WebCore::TransformationMatrix matrix;
    auto dumpMatrix = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(s, matrix); };
    const std::string expected =
        "\n  [1.00 0.00 0.00 0.00]\n  [0.00 1.00 0.00 0.00]\n  [0.00 0.00 1.00 0.00]\n  [0.00 0.00 0.00 1.00]";
    Check(Dump(dumpMatrix) == expected);

    matrix.m_matrix[0][1] = 2.0;
    matrix.m_matrix[2][3] = -0.5;
    matrix.m_matrix[3][0] = 7.25;
    Check(Dump(dumpMatrix, NumberRespectingIntegers) ==
        "\n  [1 2 0 0]\n  [0 1 0 0]\n  [0 0 1 -0.50]\n  [7.25 0 0 1]");
    Check(Dump(dumpMatrix, 0, false) ==
        "\n[1.00 2.00 0.00 0.00]\n[0.00 1.00 0.00 0.00]\n[0.00 0.00 1.00 -0.50]\n[7.25 0.00 0.00 1.00]");

    TextStream indented = NewStream();
    indented.indent = 2;
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(indented, WebCore::TransformationMatrix{});
    Check(indented.indent == 2);
    Check(Release(indented) ==
        "\n      [1.00 0.00 0.00 0.00]\n      [0.00 1.00 0.00 0.00]\n      [0.00 0.00 1.00 0.00]\n      [0.00 0.00 0.00 1.00]");
}

void TestScrollingAndLayerEnums() {
    const char* nodes[] = {"main-frame-scrolling", "subframe-scrolling", "frame-hosting", "overflow-scrolling", "overflow-scroll-proxy", "fixed", "sticky", "positioned"};
    for (int i = 0; i < 8; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(s, static_cast<WebCore::ScrollingNodeType>(i)); }) == nodes[i]);
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(s, static_cast<WebCore::ScrollingNodeType>(8)); }).empty());

    const char* actions[] = {"set", "set approximate", "sync"};
    for (int i = 0; i < 3; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(s, static_cast<WebCore::ScrollingLayerPositionAction>(i)); }) == actions[i]);

    const char* stability[] = {"stable", "unstable", "changing obscured insets interactively"};
    for (int i = 0; i < 3; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(s, static_cast<WebCore::ViewportRectStability>(i)); }) == stability[i]);

    const char* phases[] = {"background", "foreground", "mask", "clip-path", "overflow-contents", "composited-scroll", "child-clipping-mask"};
    for (int i = 0; i < 7; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(s, static_cast<WebCore::GraphicsLayerPaintingPhase>(1 << i)); }) == phases[i]);
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(s, static_cast<WebCore::GraphicsLayerPaintingPhase>(3)); }).empty());

    const char* behaviors[] = {"none", "moves", "stationary"};
    for (int i = 0; i < 3; ++i)
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(s, static_cast<WebCore::ScrollPositioningBehavior>(i)); }) == behaviors[i]);

    const char* appearances[] = {"none", "scrolling-overhang", "scrolling-shadow"};
    for (int i = 0; i < 3; ++i) {
        WebCore::GraphicsLayer::CustomAppearance appearance = static_cast<WebCore::GraphicsLayer::CustomAppearance>(i);
        Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(s, appearance); }) == appearances[i]);
    }
}

void TestTouchAction() {
    using WebCore::TouchAction;
    auto dump = [](TouchAction value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(s, value); });
    };
    Check(dump(TouchAction::Auto) == "auto");
    Check(dump(TouchAction::None) == "none");
    Check(dump(TouchAction::Manipulation) == "manipulation");
    Check(dump(TouchAction::PanX) == "pan-x");
    Check(dump(TouchAction::PanY) == "pan-y");
    Check(dump(TouchAction::PinchZoom) == "pinch-zoom");
    Check(dump(static_cast<TouchAction>(0)).empty());
    Check(dump(static_cast<TouchAction>(0x40)).empty());
    Check(dump(static_cast<TouchAction>(0x18)).empty());
}

void TestFloatRoundedRect() {
    static_assert(sizeof(WebCore::FloatRoundedRect) == 48);
    static_assert(offsetof(WebCore::FloatRoundedRect, m_radii) == 16);
    WebCore::FloatRoundedRect value{{{1.0f, 2.5f}, {3.0f, 4.0f}}, {{1.0f, 2.0f}, {3.0f, 4.0f}, {0.5f, 0.0f}, {5.0f, 6.0f}}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(s, value); };
    const std::string multiLine = "(1,2.50) width=3 height=4\n(top-left width=1 height=2)\n(top-right width=3 height=4)"
                                  "\n(bottom-left width=0.50 height=0)\n(bottom-right width=5 height=6)";
    Check(Dump(dump) == multiLine);
    Check(Dump(dump, NumberRespectingIntegers) == multiLine);
    Check(Dump(dump, 0, false) == "(1,2.50) width=3 height=4 (top-left width=1 height=2) (top-right width=3 height=4)"
                                  " (bottom-left width=0.50 height=0) (bottom-right width=5 height=6)");
    Check(Dump(dump, SVGStyleRect, false) == "at (1,2.50) size 3x4 (top-left width=1 height=2) (top-right width=3 height=4)"
                                             " (bottom-left width=0.50 height=0) (bottom-right width=5 height=6)");

    TextStream indented = NewStream();
    indented.indent = 1;
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(indented, value);
    Check(indented.indent == 1);
    Check(Release(indented) == "(1,2.50) width=3 height=4\n  (top-left width=1 height=2)\n  (top-right width=3 height=4)"
                               "\n  (bottom-left width=0.50 height=0)\n  (bottom-right width=5 height=6)");
}

void TestRectEdges() {
    static_assert(sizeof(WebCore::RectEdges<float>) == 16);
    WebCore::RectEdges<float> edges{{1.0f, 2.5f, 3.0f, 4.0f}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(s, edges); };
    Check(Dump(dump) == "[top 1.00 right 2.50 bottom 3.00 left 4.00]");
    Check(Dump(dump, NumberRespectingIntegers) == "[top 1 right 2.50 bottom 3 left 4]");
    Check(Dump(dump, 0, false) == "[top 1.00 right 2.50 bottom 3.00 left 4.00]");
    WebCore::RectEdges<float> negative{{-0.25f, 0.0f, 0.0f, -8.0f}};
    Check(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(s, negative); }, NumberRespectingIntegers)
        == "[top -0.25 right 0 bottom 0 left -8]");
}

struct RegionHolder {
    alignas(alignof(WebCore::Region)) unsigned char storage[sizeof(WebCore::Region)];
    WebCore::Region* ptr() { return reinterpret_cast<WebCore::Region*>(storage); }
};

std::string DumpRegion(WebCore::Region& region, int indent, bool multiLine) {
    TextStream stream = NewStream(0, multiLine);
    stream.indent = indent;
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(stream, region);
    Check(&returned == &stream);
    Check(stream.indent == indent);
    return Release(stream);
}

void TestRegion() {
    RegionHolder empty;
    _ZN7WebCore6RegionC1Ev(empty.ptr());
    Check(DumpRegion(*empty.ptr(), 0, true) == "\n");
    Check(DumpRegion(*empty.ptr(), 3, false) == "\n");
    _ZN7WebCore6RegionD1Ev(empty.ptr());

    RegionHolder single;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(single.ptr(), WebCore::IntRect{{1, 2}, {3, 4}});
    Check(DumpRegion(*single.ptr(), 0, true) == "\n  (rect (1,2) width=3 height=4)\n");

    RegionHolder other;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(other.ptr(), WebCore::IntRect{{20, 2}, {5, 4}});
    _ZN7WebCore6Region5uniteERKS0_(single.ptr(), *other.ptr());
    Check(DumpRegion(*single.ptr(), 0, true) == "\n  (rect (1,2) width=3 height=4)\n  (rect (20,2) width=5 height=4)\n");
    Check(DumpRegion(*single.ptr(), 1, true) == "\n    (rect (1,2) width=3 height=4)\n    (rect (20,2) width=5 height=4)\n");
    Check(DumpRegion(*single.ptr(), 1, false) == "\n(rect (1,2) width=3 height=4)\n(rect (20,2) width=5 height=4)\n");
    _ZN7WebCore6RegionD1Ev(other.ptr());
    _ZN7WebCore6RegionD1Ev(single.ptr());
}

void TestScrollableAreaParameters() {
    using WebCore::NativeScrollbarVisibility;
    using WebCore::ScrollableAreaParameters;
    static_assert(sizeof(ScrollableAreaParameters) == 11);
    static_assert(offsetof(ScrollableAreaParameters, allowsHorizontalScrolling) == 6);
    static_assert(offsetof(ScrollableAreaParameters, horizontalNativeScrollbarVisibility) == 8);
    static_assert(offsetof(ScrollableAreaParameters, useDarkAppearanceForScrollbars) == 10);
    static_assert(std::is_trivially_copyable_v<ScrollableAreaParameters>);
    auto dump = [](ScrollableAreaParameters value, bool multiLine = true) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(s, value); }, 0, multiLine);
    };

    ScrollableAreaParameters defaults;
    const std::string base = "\n(horizontal scroll elasticity 1)\n(vertical scroll elasticity 1)\n(horizontal scrollbar mode 0)\n(vertical scrollbar mode 0)";
    Check(dump(defaults) == base);
    Check(dump(defaults, false) == " (horizontal scroll elasticity 1) (vertical scroll elasticity 1) (horizontal scrollbar mode 0) (vertical scrollbar mode 0)");

    ScrollableAreaParameters ignored;
    ignored.horizontalOverscrollBehavior = WebCore::OverscrollBehavior::Contain;
    ignored.verticalOverscrollBehavior = WebCore::OverscrollBehavior::None;
    ignored.useDarkAppearanceForScrollbars = true;
    ignored.horizontalNativeScrollbarVisibility = NativeScrollbarVisibility::ReplacedByCustomScrollbar;
    ignored.verticalNativeScrollbarVisibility = NativeScrollbarVisibility::ReplacedByCustomScrollbar;
    Check(dump(ignored) == base);

    ScrollableAreaParameters all;
    all.horizontalScrollElasticity = WebCore::ScrollElasticity::Allowed;
    all.verticalScrollElasticity = WebCore::ScrollElasticity::Automatic;
    all.horizontalScrollbarMode = WebCore::ScrollbarMode::AlwaysOn;
    all.verticalScrollbarMode = WebCore::ScrollbarMode::AlwaysOff;
    all.allowsHorizontalScrolling = true;
    all.allowsVerticalScrolling = true;
    all.horizontalNativeScrollbarVisibility = NativeScrollbarVisibility::HiddenByStyle;
    all.verticalNativeScrollbarVisibility = NativeScrollbarVisibility::HiddenByStyle;
    Check(dump(all) == "\n(horizontal scroll elasticity 2)\n(vertical scroll elasticity 0)\n(horizontal scrollbar mode 2)\n(vertical scrollbar mode 1)"
                       "\n(allows horizontal scrolling 1)\n(allows vertical scrolling 1)"
                       "\n(horizontal scrollbar hidden by style 1)\n(vertical scrollbar hidden by style 1)");

    ScrollableAreaParameters vertical;
    vertical.allowsVerticalScrolling = true;
    vertical.horizontalNativeScrollbarVisibility = NativeScrollbarVisibility::HiddenByStyle;
    Check(dump(vertical) == base + "\n(allows vertical scrolling 1)\n(horizontal scrollbar hidden by style 1)");

    ScrollableAreaParameters invalid;
    invalid.horizontalScrollElasticity = static_cast<WebCore::ScrollElasticity>(3);
    invalid.verticalScrollbarMode = static_cast<WebCore::ScrollbarMode>(9);
    Check(dump(invalid) == "\n(horizontal scroll elasticity )\n(vertical scroll elasticity 1)\n(horizontal scrollbar mode 0)\n(vertical scrollbar mode )");

    TextStream indented = NewStream();
    indented.indent = 2;
    _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(indented, vertical);
    Check(indented.indent == 2);
    Check(Release(indented) == "\n    (horizontal scroll elasticity 1)\n    (vertical scroll elasticity 1)\n    (horizontal scrollbar mode 0)"
                               "\n    (vertical scrollbar mode 0)\n    (allows vertical scrolling 1)\n    (horizontal scrollbar hidden by style 1)");
}

void TestChaining() {
    TextStream stream = NewStream();
    WebCore::IntPoint point{1, 2};
    _ZN3WTF10TextStreamlsEPKc(&stream, "a=");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(stream, point);
    _ZN3WTF10TextStreamlsEPKc(&stream, " b=");
    _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(stream, WebCore::LineCap::Round);
    Check(Release(stream) == "a=(1,2) b=ROUND");
}

}  // namespace

int main() {
    TestIntegerGeometry();
    TestFloatGeometry();
    TestEnums();
    TestScrollAlignment();
    TestScrollingAndLayerEnums();
    TestLayoutValues();
    TestTransforms();
    TestTouchAction();
    TestFloatRoundedRect();
    TestRectEdges();
    TestRegion();
    TestScrollableAreaParameters();
    TestChaining();
    return 0;
}
