#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE, PAGE_NOACCESS));
        Check(memory != nullptr);
        Check(VirtualAlloc(memory, pageSize, MEM_COMMIT, PAGE_READWRITE) != nullptr);
#else
        void* mapped = mmap(nullptr, 2 * pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Check(mapped != MAP_FAILED);
        Check(mprotect(mapped, pageSize, PROT_READ | PROT_WRITE) == 0);
        memory = static_cast<std::uint8_t*>(mapped);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * pageSize);
#endif
    }

    std::uint8_t* readableEnd() { return memory + pageSize; }

    static constexpr std::size_t pageSize = 4096;

private:
    std::uint8_t* memory;
};

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

template<typename T> std::string ThrowsKeepingStream(T action, std::source_location location = std::source_location::current()) {
    TextStream stream = NewStream();
    _ZN3WTF10TextStreamlsEPKc(&stream, "kept");
    bool threw = false;
    try {
        action(stream);
    } catch (const std::exception&) {
        threw = true;
    }
    Check(threw, location);
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

void TestActivityState() {
    auto dump = [](std::uint32_t flags) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS0_9OptionSetINS_13ActivityState4FlagEEE(s, flags); });
    };
    Check(dump(0) == "");
    const char* names[] = {"active window", "focused", "visible", "visible or occluded", "in-window", "visually idle", "audible", "loading", "capturing media"};
    std::string all;
    for (unsigned bit = 0; bit < 9; ++bit) {
        Check(dump(1u << bit) == names[bit]);
        if (bit) all += ", ";
        all += names[bit];
    }
    Check(dump(0x1FF) == all);
    Check(dump(0xFFFFFFFFu) == all);
    Check(dump((1u << 1) | (1u << 6)) == "focused, audible");
    Check(dump((1u << 8) | (1u << 0)) == "active window, capturing media");
    Check(dump(1u << 9) == "");
    Check(dump((1u << 9) | (1u << 7)) == "loading");
}

void TestTouchAction() {
    auto dump = [](unsigned value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(s, static_cast<WebCore::TouchAction>(value)); });
    };
    Check(dump(1) == "auto");
    Check(dump(2) == "none");
    Check(dump(4) == "manipulation");
    Check(dump(8) == "pan-x");
    Check(dump(16) == "pan-y");
    Check(dump(32) == "pinch-zoom");
    for (unsigned value : {0u, 3u, 24u, 64u, 128u, 255u}) Check(dump(value) == "");
}

void TestRegion() {
    struct Storage {
        alignas(alignof(WebCore::Region)) char storage[sizeof(WebCore::Region)];
        WebCore::Region* ptr() { return reinterpret_cast<WebCore::Region*>(storage); }
    };
    auto dump = [](WebCore::Region* region, unsigned flags = 0, bool multiLine = true) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(s, *region); }, flags, multiLine);
    };
    Storage empty;
    _ZN7WebCore6RegionC1Ev(empty.ptr());
    Check(dump(empty.ptr()) == "\n");
    Storage one;
    WebCore::IntRect rect{{1, 2}, {3, 4}};
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(one.ptr(), rect);
    Check(dump(one.ptr()) == "\n  (rect (1,2) width=3 height=4)\n");
    Check(dump(one.ptr(), SVGStyleRect) == "\n  (rect at (1,2) size 3x4)\n");
    Check(dump(one.ptr(), 0, false) == "\n(rect (1,2) width=3 height=4)\n");
    Storage shape;
    WebCore::IntRect top{{0, 0}, {10, 10}};
    WebCore::IntRect bottom{{0, 10}, {5, 5}};
    Storage other;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(shape.ptr(), top);
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(other.ptr(), bottom);
    _ZN7WebCore6Region5uniteERKS0_(shape.ptr(), *other.ptr());
    Check(dump(shape.ptr()) == "\n  (rect (0,0) width=10 height=10)\n  (rect (0,10) width=5 height=5)\n");
    TextStream nested = NewStream();
    nested.indent = 2;
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(nested, *shape.ptr());
    Check(&returned == &nested);
    Check(nested.indent == 2);
    Check(Release(nested) == "\n      (rect (0,0) width=10 height=10)\n      (rect (0,10) width=5 height=5)\n");
    for (Storage* region : {&empty, &one, &shape, &other}) _ZN7WebCore6RegionD1Ev(region->ptr());
}

void TestPositionConstraints() {
    WebCore::AbsolutePositionConstraints absolute{{7.0f, 8.0f}, {1.0f, 2.5f}};
    auto dumpAbsolute = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE(s, absolute); };
    Check(Dump(dumpAbsolute) == "\n(layer-position-at-last-layout (1,2.50))");
    Check(Dump(dumpAbsolute, 0, false) == " (layer-position-at-last-layout (1,2.50))");
    WebCore::FixedPositionViewportConstraints fixed{nullptr, {7.0f, 8.0f}, 5, {{1.0f, 2.5f}, {3.0f, 4.0f}}, {-1.5f, 2.0f}};
    auto dumpFixed = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(s, fixed); };
    Check(Dump(dumpFixed) == "\n(viewport-rect-at-last-layout (1,2.50) width=3 height=4)\n(layer-position-at-last-layout (-1.50,2))");
    Check(Dump(dumpFixed, SVGStyleRect) == "\n(viewport-rect-at-last-layout at (1,2.50) size 3x4)\n(layer-position-at-last-layout (-1.50,2))");
    Check(Dump(dumpFixed, 0, false) == " (viewport-rect-at-last-layout (1,2.50) width=3 height=4) (layer-position-at-last-layout (-1.50,2))");
    TextStream nested = NewStream();
    nested.indent = 2;
    TextStream& returned = _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(nested, fixed);
    Check(&returned == &nested);
    Check(nested.indent == 2);
    Check(Release(nested) == "\n    (viewport-rect-at-last-layout (1,2.50) width=3 height=4)\n    (layer-position-at-last-layout (-1.50,2))");
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

void TestVectorOfUnsignedLong() {
    auto dump = [](const NKWebKit::WtfAbi::Vector<std::uint64_t>& vector, unsigned limit = 0) {
        return Dump([&](TextStream& s) -> TextStream& {
            s.containerSizeLimit = limit;
            return _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(s, vector);
        });
    };
    NKWebKit::WtfAbi::Vector<std::uint64_t> empty;
    Check(dump(empty) == "[]");

    NKWebKit::WtfAbi::Vector<std::uint64_t> vector(3);
    vector.size = 3;
    vector[0] = 5;
    vector[1] = 7;
    vector[2] = 9;
    Check(dump(vector) == "[5, 7, 9]");
    Check(dump(vector, 1) == "[5, ...]");
    Check(dump(vector, 2) == "[5, 7, ...]");
    Check(dump(vector, 3) == "[5, 7, 9]");
    Check(dump(vector, 4) == "[5, 7, 9]");

    NKWebKit::WtfAbi::Vector<std::uint64_t> chunked(100);
    chunked.size = 100;
    for (unsigned i = 0; i < 100; ++i) chunked[i] = i;
    std::string expected = "[0";
    for (unsigned i = 1; i < 100; ++i) expected += ", " + std::to_string(i);
    expected += "]";
    Check(dump(chunked) == expected);
    std::string truncated = "[0";
    for (unsigned i = 1; i < 70; ++i) truncated += ", " + std::to_string(i);
    truncated += ", ...]";
    Check(dump(chunked, 70) == truncated);

    NKWebKit::WtfAbi::Vector<std::uint64_t> broken(1);
    broken.buffer = nullptr;
    broken.size = 1;
    Throws([&] { dump(broken); });
}

void TestVectorThrowKeepsStream() {
    GuardPage guard;
    NKWebKit::WtfAbi::Vector<std::uint64_t> vector;
    vector.buffer = reinterpret_cast<std::uint64_t*>(guard.readableEnd() - 64 * sizeof(std::uint64_t));
    vector.capacity = 100;
    vector.size = 100;
    for (unsigned i = 0; i < 64; ++i) vector.buffer[i] = i + 1;
    std::string kept = ThrowsKeepingStream([&](TextStream& s) -> TextStream& {
        return _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(s, vector);
    });
    Check(kept == "kept");
    vector.buffer = nullptr;
}

void TestScrollableAreaParameters() {
    auto dump = [](WebCore::ScrollableAreaParameters value) {
        return Dump([&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(s, value); });
    };
    WebCore::ScrollableAreaParameters parameters;
    Check(dump(parameters) ==
        "\n(horizontal scroll elasticity 1)\n(vertical scroll elasticity 1)\n(horizontal scrollbar mode 0)\n(vertical scrollbar mode 0)");
    parameters.horizontalScrollElasticity = WebCore::ScrollElasticity::Automatic;
    parameters.verticalScrollElasticity = WebCore::ScrollElasticity::Allowed;
    parameters.horizontalScrollbarMode = WebCore::ScrollbarMode::AlwaysOff;
    parameters.verticalScrollbarMode = WebCore::ScrollbarMode::AlwaysOn;
    Check(dump(parameters) ==
        "\n(horizontal scroll elasticity 0)\n(vertical scroll elasticity 2)\n(horizontal scrollbar mode 1)\n(vertical scrollbar mode 2)");
    parameters.allowsHorizontalScrolling = true;
    parameters.allowsVerticalScrolling = true;
    parameters.horizontalNativeScrollbarVisibility = WebCore::NativeScrollbarVisibility::HiddenByStyle;
    parameters.verticalNativeScrollbarVisibility = WebCore::NativeScrollbarVisibility::HiddenByStyle;
    Check(dump(parameters) ==
        "\n(horizontal scroll elasticity 0)\n(vertical scroll elasticity 2)\n(horizontal scrollbar mode 1)\n(vertical scrollbar mode 2)"
        "\n(allows horizontal scrolling 1)\n(allows vertical scrolling 1)"
        "\n(horizontal scrollbar hidden by style 1)\n(vertical scrollbar hidden by style 1)");
    parameters.horizontalNativeScrollbarVisibility = WebCore::NativeScrollbarVisibility::Visible;
    Check(dump(parameters) ==
        "\n(horizontal scroll elasticity 0)\n(vertical scroll elasticity 2)\n(horizontal scrollbar mode 1)\n(vertical scrollbar mode 2)"
        "\n(allows horizontal scrolling 1)\n(allows vertical scrolling 1)"
        "\n(vertical scrollbar hidden by style 1)");
}

std::string PointerText(const void* pointer) {
    TextStream stream = NewStream();
    _ZN3WTF10TextStreamlsEPKv(&stream, pointer);
    return Release(stream);
}

std::string PositionDumpText(const char* label, const void* position, const void* anchorNode, const std::string& offset, const char* anchorType) {
    return std::string("\n  (") + label + " \n    (Position " + PointerText(position) +
        "\n      (anchor node " + PointerText(anchorNode) + ")" +
        "\n      (offset " + offset + ")" +
        "\n      (anchor type " + anchorType + ")))";
}

void TestVisibleSelectionDump() {
    WebCore::VisibleSelection selection;
    selection.m_base.m_anchorNode = reinterpret_cast<void*>(0x2001);
    selection.m_base.m_offset = 1;
    selection.m_base.m_anchorTypeBits = 0;
    selection.m_extent.m_anchorNode = reinterpret_cast<void*>(0x2002);
    selection.m_extent.m_offset = 2;
    selection.m_extent.m_anchorTypeBits = 1;
    selection.m_start.m_anchorNode = reinterpret_cast<void*>(0x2003);
    selection.m_start.m_offset = 3;
    selection.m_start.m_anchorTypeBits = 3;
    selection.m_end.m_anchorNode = reinterpret_cast<void*>(0x2004);
    selection.m_end.m_offset = 0xFFFFFFFF;
    selection.m_end.m_anchorTypeBits = 4;
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_16VisibleSelectionE(s, selection); };
    std::string expected = "\n(VisibleSelection " + PointerText(&selection) +
        PositionDumpText("base", &selection.m_base, reinterpret_cast<void*>(0x2001), "1", "offset in anchor") +
        PositionDumpText("extent", &selection.m_extent, reinterpret_cast<void*>(0x2002), "2", "before anchor") +
        PositionDumpText("start", &selection.m_start, reinterpret_cast<void*>(0x2003), "3", "before children") +
        PositionDumpText("end", &selection.m_end, reinterpret_cast<void*>(0x2004), "-1", "after children") + ")";
    Check(Dump(dump) == expected);
}

void TestVisiblePositionDump() {
    WebCore::VisiblePosition visiblePosition;
    visiblePosition.m_deepPosition.m_anchorNode = reinterpret_cast<void*>(0x1000);
    visiblePosition.m_deepPosition.m_offset = 5;
    visiblePosition.m_deepPosition.m_anchorTypeBits = 0;
    visiblePosition.m_affinity = 1;
    auto dump = [&](TextStream& s) -> TextStream& { return _ZN7WebCorelsERN3WTF10TextStreamERKNS_15VisiblePositionE(s, visiblePosition); };
    std::string result = Dump(dump);
    Check(result.rfind("\n(VisiblePosition " + PointerText(&visiblePosition) + "\n  (position \n    (Position ", 0) == 0);
    Check(result.find("\n      (anchor node " + PointerText(reinterpret_cast<void*>(0x1000)) + ")") != std::string::npos);
    Check(result.find("\n      (offset 5)") != std::string::npos);
    Check(result.find("\n      (anchor type offset in anchor)))") != std::string::npos);
    Check(result.find("\n  (affinity downstream))") != std::string::npos);

    visiblePosition.m_affinity = 0;
    Check(Dump(dump).find("\n  (affinity upstream))") != std::string::npos);
    visiblePosition.m_affinity = 7;
    Check(Dump(dump).find("\n  (affinity ))") != std::string::npos);
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
    TestActivityState();
    TestTouchAction();
    TestRegion();
    TestPositionConstraints();
    TestChaining();
    TestVectorOfUnsignedLong();
    TestVectorThrowKeepsStream();
    TestScrollableAreaParameters();
    TestVisiblePositionDump();
    TestVisibleSelectionDump();
    return 0;
}
