#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"
#include "Testing.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::TextStream;

constexpr unsigned SVGStyleRect = 1;
constexpr unsigned NumberRespectingIntegers = 2;
constexpr unsigned LayoutUnitsAsIntegers = 4;

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
    Require(released.impl->hashAndFlags & 4);
    for (unsigned i = 0; i < released.impl->length; ++i) result.push_back(static_cast<char>(static_cast<const unsigned char*>(released.impl->data)[i]));
    return result;
}

template<typename Function> std::string Dump(Function function, unsigned flags = 0, bool multiLine = true) {
    TextStream stream = NewStream(flags, multiLine);
    TextStream& returned = function(stream);
    Require(&returned == &stream);
    Require(stream.formattingFlags == flags);
    return Release(stream);
}

void TestIntegerGeometry() {
    WebCore::IntPoint point{3, -4};
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(s, point); }) == "(3,-4)");
    WebCore::IntSize size{5, 6};
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(s, size); }) == "width=5 height=6");
    WebCore::IntRect rect{{1, 2}, {3, 4}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(s, rect); };
    Require(Dump(dump) == "(1,2) width=3 height=4");
    Require(Dump(dump, SVGStyleRect) == "at (1,2) size 3x4");
    Require(Dump(dump, NumberRespectingIntegers) == "(1,2) width=3 height=4");
}

void TestFloatGeometry() {
    auto point = [](WebCore::FloatPoint value, unsigned flags = 0) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(s, value); }, flags);
    };
    Require(point({1.0f, 2.5f}) == "(1,2.50)");
    Require(point({-1.5f, 2.0f}) == "(-1.50,2)");
    Require(point({0.00001f, 3.0f}) == "(0,3)");
    Require(point({1.0f, 2.5f}, SVGStyleRect | NumberRespectingIntegers) == "(1,2.50)");

    auto size = [](WebCore::FloatSize value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(s, value); });
    };
    Require(size({3.0f, 0.25f}) == "width=3 height=0.25");

    WebCore::FloatRect rect{{1.0f, 2.5f}, {3.0f, 4.0f}};
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(s, rect); };
    Require(Dump(dump) == "(1,2.50) width=3 height=4");
    Require(Dump(dump, SVGStyleRect) == "at (1,2.50) size 3x4");

    WebCore::FloatPoint3D point3{1.0f, 2.5f, -3.0f};
    auto dump3 = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_12FloatPoint3DE(s, point3); };
    Require(Dump(dump3) == "1.00 2.50 -3.00");
    Require(Dump(dump3, NumberRespectingIntegers) == "1 2.50 -3");
}

void TestEnums() {
    auto composite = [](WebCore::CompositeOperator value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17CompositeOperatorE(s, value); });
    };
    Require(composite(WebCore::CompositeOperator::Clear) == "clear");
    Require(composite(WebCore::CompositeOperator::SourceOver) == "source-over");
    Require(composite(WebCore::CompositeOperator::XOR) == "xor");
    Require(composite(WebCore::CompositeOperator::PlusDarker) == "darker");
    Require(composite(WebCore::CompositeOperator::PlusLighter) == "lighter");
    Require(composite(WebCore::CompositeOperator::Difference) == "difference");
    ExpectErr<std::invalid_argument>([&] { composite(static_cast<WebCore::CompositeOperator>(14)); });

    auto blend = [](WebCore::BlendMode value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_9BlendModeE(s, value); });
    };
    Require(blend(WebCore::BlendMode::Normal) == "normal");
    Require(blend(WebCore::BlendMode::ColorDodge) == "color-dodge");
    Require(blend(WebCore::BlendMode::Difference) == "difference");
    Require(blend(WebCore::BlendMode::PlusLighter) == "plus-lighter");
    ExpectErr<std::invalid_argument>([&] { blend(static_cast<WebCore::BlendMode>(0)); });
    ExpectErr<std::invalid_argument>([&] { blend(static_cast<WebCore::BlendMode>(19)); });

    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(s, WebCore::WindRule::NonZero); }) == "NON-ZERO");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(s, WebCore::WindRule::EvenOdd); }) == "EVEN-ODD");

    const char* caps[] = {"BUTT", "ROUND", "SQUARE"};
    for (int i = 0; i < 3; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(s, static_cast<WebCore::LineCap>(i)); }) == caps[i]);
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(s, static_cast<WebCore::LineCap>(3)); }).empty());

    const char* joins[] = {"MITER", "ROUND", "BEVEL"};
    for (int i = 0; i < 3; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_8LineJoinE(s, static_cast<WebCore::LineJoin>(i)); }) == joins[i]);

    const char* spaces[] = {"A98-RGB", "DisplayP3", "Extended A98-RGB", "Extended DisplayP3", "Extended Linear sRGB", "Extended ProPhotoRGB",
                            "Extended Rec2020", "Extended sRGB", "HSL", "HWB", "LCH", "Lab", "Linear sRGB", "OKLCH", "OKLab",
                            "ProPhotoRGB", "Rec2020", "sRGB", "XYZ-D50", "XYZ-D50"};
    for (int i = 0; i < 20; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(s, static_cast<WebCore::ColorSpace>(i)); }) == spaces[i]);
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(s, static_cast<WebCore::ColorSpace>(20)); }).empty());

    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(s, WebCore::AlphaPremultiplication::Premultiplied); }) == "premultiplied");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(s, WebCore::AlphaPremultiplication::Unpremultiplied); }) == "unpremultiplied");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(s, WebCore::ScrollType::User); }) == "user");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(s, WebCore::ScrollType::Programmatic); }) == "programmatic");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(s, WebCore::ScrollClamping::Unclamped); }) == "unclamped");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(s, WebCore::ScrollClamping::Clamped); }) == "clamped");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(s, WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds); }) == "0");
    Require(Dump([](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(s, WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds); }) == "1");
}

void TestScrollAlignment() {
    using Behavior = WebCore::ScrollAlignment::Behavior;
    const char* names[] = {"NoScroll", "AlignCenter", "AlignTop", "AlignBottom", "AlignLeft", "AlignRight", "AlignToClosestEdge"};
    for (int i = 0; i < 7; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(s, static_cast<Behavior>(i)); }) == names[i]);
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(s, static_cast<Behavior>(99)); }).empty());

    WebCore::ScrollAlignment alignment{Behavior::NoScroll, Behavior::AlignCenter, Behavior::AlignToClosestEdge};
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15ScrollAlignmentE(s, alignment); })
        == "ScrollAlignment: visible: NoScroll hidden: AlignCenter partial: AlignToClosestEdge");
}

void TestLayoutValues() {
    auto unit = [](int raw, unsigned flags = 0) {
        WebCore::LayoutUnit value{raw};
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutUnitE(s, value); }, flags);
    };
    Require(unit(96) == "1.50");
    Require(unit(128) == "2");
    Require(unit(-96) == "-1.50");
    Require(unit(0) == "0");

    WebCore::LayoutSize size{{96}, {128}};
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutSizeE(s, size); }) == "width=1.50 height=2.00");

    WebCore::LayoutPoint point{{96}, {-96}};
    auto dumpPoint = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_11LayoutPointE(s, point); };
    Require(Dump(dumpPoint) == "(1.50,-1.50)");
    Require(Dump(dumpPoint, LayoutUnitsAsIntegers) == "(1,-1)");

    WebCore::LayoutRect rect{{{96}, {160}}, {{192}, {64}}};
    auto dumpRect = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(s, rect); };
    Require(Dump(dumpRect) == "(1.50,2.50) width=3 height=1");
    Require(Dump(dumpRect, SVGStyleRect) == "at (1.50,2.50) size 3x1");
    Require(Dump(dumpRect, LayoutUnitsAsIntegers) == "(2,3) width=3 height=1");
    Require(Dump(dumpRect, LayoutUnitsAsIntegers | SVGStyleRect) == "at (2,3) size 3x1");

    WebCore::LayoutRect negative{{{-96}, {0}}, {{64}, {64}}};
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(s, negative); }, LayoutUnitsAsIntegers) == "(-1,0) width=1 height=1");
}

void TestTransforms() {
    WebCore::AffineTransform identity;
    auto dumpIdentity = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(s, identity); };
    Require(Dump(dumpIdentity) == "identity");

    WebCore::AffineTransform transform;
    transform.m_transform = {2.0, 0.0, 0.0, 3.0, 10.5, -1.0};
    auto dumpTransform = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(s, transform); };
    Require(Dump(dumpTransform) == "{m=((2.00,0.00)(0.00,3.00)) t=(10.50,-1.00)}");
    Require(Dump(dumpTransform, NumberRespectingIntegers) == "{m=((2,0)(0,3)) t=(10.50,-1)}");
    transform.m_transform = {1.0, 0.0, 0.0, 1.0, 0.0, 0.5};
    Require(Dump(dumpTransform) == "{m=((1.00,0.00)(0.00,1.00)) t=(0.00,0.50)}");

    WebCore::TransformationMatrix matrix;
    auto dumpMatrix = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(s, matrix); };
    const std::string expected =
        "\n  [1.00 0.00 0.00 0.00]\n  [0.00 1.00 0.00 0.00]\n  [0.00 0.00 1.00 0.00]\n  [0.00 0.00 0.00 1.00]";
    Require(Dump(dumpMatrix) == expected);

    matrix.m_matrix[0][1] = 2.0;
    matrix.m_matrix[2][3] = -0.5;
    matrix.m_matrix[3][0] = 7.25;
    Require(Dump(dumpMatrix, NumberRespectingIntegers) ==
        "\n  [1 2 0 0]\n  [0 1 0 0]\n  [0 0 1 -0.50]\n  [7.25 0 0 1]");
    Require(Dump(dumpMatrix, 0, false) ==
        "\n[1.00 2.00 0.00 0.00]\n[0.00 1.00 0.00 0.00]\n[0.00 0.00 1.00 -0.50]\n[7.25 0.00 0.00 1.00]");

    TextStream indented = NewStream();
    indented.indent = 2;
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(indented, WebCore::TransformationMatrix{});
    Require(indented.indent == 2);
    Require(Release(indented) ==
        "\n      [1.00 0.00 0.00 0.00]\n      [0.00 1.00 0.00 0.00]\n      [0.00 0.00 1.00 0.00]\n      [0.00 0.00 0.00 1.00]");
}

void TestScrollingAndLayerEnums() {
    const char* nodes[] = {"main-frame-scrolling", "subframe-scrolling", "frame-hosting", "overflow-scrolling", "overflow-scroll-proxy", "fixed", "sticky", "positioned"};
    for (int i = 0; i < 8; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(s, static_cast<WebCore::ScrollingNodeType>(i)); }) == nodes[i]);
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(s, static_cast<WebCore::ScrollingNodeType>(8)); }).empty());

    const char* actions[] = {"set", "set approximate", "sync"};
    for (int i = 0; i < 3; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(s, static_cast<WebCore::ScrollingLayerPositionAction>(i)); }) == actions[i]);

    const char* stability[] = {"stable", "unstable", "changing obscured insets interactively"};
    for (int i = 0; i < 3; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(s, static_cast<WebCore::ViewportRectStability>(i)); }) == stability[i]);

    const char* phases[] = {"background", "foreground", "mask", "clip-path", "overflow-contents", "composited-scroll", "child-clipping-mask"};
    for (int i = 0; i < 7; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(s, static_cast<WebCore::GraphicsLayerPaintingPhase>(1 << i)); }) == phases[i]);
    Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(s, static_cast<WebCore::GraphicsLayerPaintingPhase>(3)); }).empty());

    const char* behaviors[] = {"none", "moves", "stationary"};
    for (int i = 0; i < 3; ++i)
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(s, static_cast<WebCore::ScrollPositioningBehavior>(i)); }) == behaviors[i]);

    const char* appearances[] = {"none", "scrolling-overhang", "scrolling-shadow"};
    for (int i = 0; i < 3; ++i) {
        WebCore::GraphicsLayer::CustomAppearance appearance = static_cast<WebCore::GraphicsLayer::CustomAppearance>(i);
        Require(Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(s, appearance); }) == appearances[i]);
    }
}

void TestChaining() {
    TextStream stream = NewStream();
    WebCore::IntPoint point{1, 2};
    _ZN3WTF10TextStreamlsEPKc(&stream, "a=");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(stream, point);
    _ZN3WTF10TextStreamlsEPKc(&stream, " b=");
    _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(stream, WebCore::LineCap::Round);
    Require(Release(stream) == "a=(1,2) b=ROUND");
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
    TestChaining();
    return 0;
}
