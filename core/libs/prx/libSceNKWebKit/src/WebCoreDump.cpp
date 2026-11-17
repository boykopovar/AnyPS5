#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <climits>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::TextStream;

constexpr unsigned SVGStyleRect = 1u << 0;
constexpr unsigned NumberRespectingIntegers = 1u << 1;
constexpr unsigned LayoutUnitsAsIntegers = 1u << 2;
constexpr int FixedPointDenominator = 64;
constexpr int LayoutUnitFractionalBits = 6;

[[noreturn]] void ThrowBadValue() {
    throw std::invalid_argument("invalid WebCore enumeration value");
}

TextStream& Text(TextStream& stream, const char* text) {
    return _ZN3WTF10TextStreamlsEPKc(&stream, text);
}

TextStream& Int(TextStream& stream, int value) {
    return _ZN3WTF10TextStreamlsEi(&stream, value);
}

TextStream& Float(TextStream& stream, float value) {
    return _ZN3WTF10TextStreamlsEf(&stream, value);
}

TextStream& Double(TextStream& stream, double value) {
    return _ZN3WTF10TextStreamlsEd(&stream, value);
}

bool HasFlag(const TextStream& stream, unsigned flag) {
    return (stream.formattingFlags & flag) != 0;
}

TextStream& RespectingIntegers(TextStream& stream, double value) {
    struct Restore {
        TextStream& stream;
        unsigned flags;
        ~Restore() { stream.formattingFlags = flags; }
    } restore{stream, stream.formattingFlags};
    stream.formattingFlags |= NumberRespectingIntegers;
    return Double(stream, value);
}

float ToFloat(WebCore::LayoutUnit value) {
    return static_cast<float>(value.m_value) / FixedPointDenominator;
}

int SaturatedSum(int a, int b) {
    long long sum = static_cast<long long>(a) + b;
    if (sum > INT_MAX) return INT_MAX;
    if (sum < INT_MIN) return INT_MIN;
    return static_cast<int>(sum);
}

int Round(WebCore::LayoutUnit value) {
    return SaturatedSum(value.m_value, FixedPointDenominator / 2) >> LayoutUnitFractionalBits;
}

int SnapSize(WebCore::LayoutUnit size, WebCore::LayoutUnit location) {
    WebCore::LayoutUnit fraction{location.m_value % FixedPointDenominator};
    return Round(WebCore::LayoutUnit{SaturatedSum(fraction.m_value, size.m_value)}) - Round(fraction);
}

const char* const kCompositeOperatorNames[] = {
    "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over",
    "destination-in", "destination-out", "destination-atop", "xor", "darker", "lighter", "difference"
};

const char* const kBlendModeNames[] = {
    "normal", "multiply", "screen", "darken", "lighten", "overlay", "color-dodge", "color-burn", "hard-light",
    "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity", "plus-darker", "plus-lighter"
};

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(TextStream& stream, const WebCore::IntPoint& value) {
    Text(stream, "(");
    Int(stream, value.m_x);
    Text(stream, ",");
    Int(stream, value.m_y);
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(TextStream& stream, const WebCore::IntSize& value) {
    Text(stream, "width=");
    Int(stream, value.m_width);
    Text(stream, " height=");
    return Int(stream, value.m_height);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(TextStream& stream, const WebCore::IntRect& value) {
    if (HasFlag(stream, SVGStyleRect)) {
        Text(stream, "at (");
        Int(stream, value.m_location.m_x);
        Text(stream, ",");
        Int(stream, value.m_location.m_y);
        Text(stream, ") size ");
        Int(stream, value.m_size.m_width);
        Text(stream, "x");
        return Int(stream, value.m_size.m_height);
    }
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(stream, value.m_location);
    Text(stream, " ");
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(stream, value.m_size);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(TextStream& stream, const WebCore::FloatPoint& value) {
    Text(stream, "(");
    RespectingIntegers(stream, value.m_x);
    Text(stream, ",");
    RespectingIntegers(stream, value.m_y);
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(TextStream& stream, const WebCore::FloatSize& value) {
    Text(stream, "width=");
    RespectingIntegers(stream, value.m_width);
    Text(stream, " height=");
    return RespectingIntegers(stream, value.m_height);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(TextStream& stream, const WebCore::FloatRect& value) {
    if (HasFlag(stream, SVGStyleRect)) {
        Text(stream, "at (");
        RespectingIntegers(stream, value.m_location.m_x);
        Text(stream, ",");
        RespectingIntegers(stream, value.m_location.m_y);
        Text(stream, ") size ");
        RespectingIntegers(stream, value.m_size.m_width);
        Text(stream, "x");
        return RespectingIntegers(stream, value.m_size.m_height);
    }
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(stream, value.m_location);
    Text(stream, " ");
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(stream, value.m_size);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12FloatPoint3DE(TextStream& stream, const WebCore::FloatPoint3D& value) {
    Float(stream, value.m_x);
    Text(stream, " ");
    Float(stream, value.m_y);
    Text(stream, " ");
    return Float(stream, value.m_z);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutUnitE(TextStream& stream, const WebCore::LayoutUnit& value) {
    return RespectingIntegers(stream, ToFloat(value));
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutSizeE(TextStream& stream, const WebCore::LayoutSize& value) {
    Text(stream, "width=");
    Float(stream, ToFloat(value.m_width));
    Text(stream, " height=");
    return Float(stream, ToFloat(value.m_height));
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_11LayoutPointE(TextStream& stream, const WebCore::LayoutPoint& value) {
    Text(stream, "(");
    if (HasFlag(stream, LayoutUnitsAsIntegers)) {
        Int(stream, value.m_x.m_value / FixedPointDenominator);
        Text(stream, ",");
        Int(stream, value.m_y.m_value / FixedPointDenominator);
    } else {
        Float(stream, ToFloat(value.m_x));
        Text(stream, ",");
        Float(stream, ToFloat(value.m_y));
    }
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(TextStream& stream, const WebCore::LayoutRect& value) {
    if (HasFlag(stream, LayoutUnitsAsIntegers)) {
        WebCore::IntRect snapped{
            WebCore::IntPoint{Round(value.m_location.m_x), Round(value.m_location.m_y)},
            WebCore::IntSize{SnapSize(value.m_size.m_width, value.m_location.m_x), SnapSize(value.m_size.m_height, value.m_location.m_y)}};
        return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(stream, snapped);
    }
    WebCore::FloatRect rect{
        WebCore::FloatPoint{ToFloat(value.m_location.m_x), ToFloat(value.m_location.m_y)},
        WebCore::FloatSize{ToFloat(value.m_size.m_width), ToFloat(value.m_size.m_height)}};
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(stream, rect);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17CompositeOperatorE(TextStream& stream, WebCore::CompositeOperator value) {
    auto index = static_cast<unsigned>(value);
    if (index >= sizeof(kCompositeOperatorNames) / sizeof(kCompositeOperatorNames[0])) ThrowBadValue();
    return Text(stream, kCompositeOperatorNames[index]);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_9BlendModeE(TextStream& stream, WebCore::BlendMode value) {
    auto index = static_cast<unsigned>(value) - static_cast<unsigned>(WebCore::BlendMode::Normal);
    if (index >= sizeof(kBlendModeNames) / sizeof(kBlendModeNames[0])) ThrowBadValue();
    return Text(stream, kBlendModeNames[index]);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(TextStream& stream, WebCore::WindRule value) {
    switch (value) {
    case WebCore::WindRule::NonZero:
        Text(stream, "NON-ZERO");
        break;
    case WebCore::WindRule::EvenOdd:
        Text(stream, "EVEN-ODD");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(TextStream& stream, WebCore::LineCap value) {
    switch (value) {
    case WebCore::LineCap::Butt:
        Text(stream, "BUTT");
        break;
    case WebCore::LineCap::Round:
        Text(stream, "ROUND");
        break;
    case WebCore::LineCap::Square:
        Text(stream, "SQUARE");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8LineJoinE(TextStream& stream, WebCore::LineJoin value) {
    switch (value) {
    case WebCore::LineJoin::Miter:
        Text(stream, "MITER");
        break;
    case WebCore::LineJoin::Round:
        Text(stream, "ROUND");
        break;
    case WebCore::LineJoin::Bevel:
        Text(stream, "BEVEL");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(TextStream& stream, WebCore::ColorSpace value) {
    using WebCore::ColorSpace;
    switch (value) {
    case ColorSpace::A98RGB: return Text(stream, "A98-RGB");
    case ColorSpace::DisplayP3: return Text(stream, "DisplayP3");
    case ColorSpace::ExtendedA98RGB: return Text(stream, "Extended A98-RGB");
    case ColorSpace::ExtendedDisplayP3: return Text(stream, "Extended DisplayP3");
    case ColorSpace::ExtendedLinearSRGB: return Text(stream, "Extended Linear sRGB");
    case ColorSpace::ExtendedProPhotoRGB: return Text(stream, "Extended ProPhotoRGB");
    case ColorSpace::ExtendedRec2020: return Text(stream, "Extended Rec2020");
    case ColorSpace::ExtendedSRGB: return Text(stream, "Extended sRGB");
    case ColorSpace::HSL: return Text(stream, "HSL");
    case ColorSpace::HWB: return Text(stream, "HWB");
    case ColorSpace::LCH: return Text(stream, "LCH");
    case ColorSpace::Lab: return Text(stream, "Lab");
    case ColorSpace::LinearSRGB: return Text(stream, "Linear sRGB");
    case ColorSpace::OKLCH: return Text(stream, "OKLCH");
    case ColorSpace::OKLab: return Text(stream, "OKLab");
    case ColorSpace::ProPhotoRGB: return Text(stream, "ProPhotoRGB");
    case ColorSpace::Rec2020: return Text(stream, "Rec2020");
    case ColorSpace::SRGB: return Text(stream, "sRGB");
    case ColorSpace::XYZ_D50: return Text(stream, "XYZ-D50");
    case ColorSpace::XYZ_D65: return Text(stream, "XYZ-D50");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(TextStream& stream, WebCore::AlphaPremultiplication value) {
    switch (value) {
    case WebCore::AlphaPremultiplication::Premultiplied:
        Text(stream, "premultiplied");
        break;
    case WebCore::AlphaPremultiplication::Unpremultiplied:
        Text(stream, "unpremultiplied");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(TextStream& stream, WebCore::ScrollType value) {
    switch (value) {
    case WebCore::ScrollType::User:
        Text(stream, "user");
        break;
    case WebCore::ScrollType::Programmatic:
        Text(stream, "programmatic");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(TextStream& stream, WebCore::ScrollClamping value) {
    switch (value) {
    case WebCore::ScrollClamping::Unclamped:
        Text(stream, "unclamped");
        break;
    case WebCore::ScrollClamping::Clamped:
        Text(stream, "clamped");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(TextStream& stream, WebCore::ScrollBehaviorForFixedElements value) {
    switch (value) {
    case WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds:
        Int(stream, 0);
        break;
    case WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds:
        Int(stream, 1);
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(TextStream& stream, WebCore::ScrollAlignment::Behavior value) {
    using Behavior = WebCore::ScrollAlignment::Behavior;
    switch (value) {
    case Behavior::NoScroll: return Text(stream, "NoScroll");
    case Behavior::AlignCenter: return Text(stream, "AlignCenter");
    case Behavior::AlignTop: return Text(stream, "AlignTop");
    case Behavior::AlignBottom: return Text(stream, "AlignBottom");
    case Behavior::AlignLeft: return Text(stream, "AlignLeft");
    case Behavior::AlignRight: return Text(stream, "AlignRight");
    case Behavior::AlignToClosestEdge: return Text(stream, "AlignToClosestEdge");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15ScrollAlignmentE(TextStream& stream, const WebCore::ScrollAlignment& value) {
    Text(stream, "ScrollAlignment: visible: ");
    _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectVisible);
    Text(stream, " hidden: ");
    _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectHidden);
    Text(stream, " partial: ");
    return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectPartial);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(TextStream& stream, const WebCore::AffineTransform& value) {
    const auto& m = value.m_transform;
    if (m[0] == 1 && m[1] == 0 && m[2] == 0 && m[3] == 1 && m[4] == 0 && m[5] == 0) return Text(stream, "identity");
    Text(stream, "{m=((");
    Double(stream, m[0]);
    Text(stream, ",");
    Double(stream, m[1]);
    Text(stream, ")(");
    Double(stream, m[2]);
    Text(stream, ",");
    Double(stream, m[3]);
    Text(stream, ")) t=(");
    Double(stream, m[4]);
    Text(stream, ",");
    Double(stream, m[5]);
    return Text(stream, ")}");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(TextStream& stream, const WebCore::TransformationMatrix& value) {
    struct IndentScope {
        TextStream& stream;
        explicit IndentScope(TextStream& s) : stream(s) { ++stream.indent; }
        ~IndentScope() { --stream.indent; }
    } scope{stream};
    Text(stream, "\n");
    for (int row = 0; row < 4; ++row) {
        _ZN3WTF10TextStream11writeIndentEv(&stream);
        Text(stream, "[");
        for (int column = 0; column < 4; ++column) {
            if (column) Text(stream, " ");
            Double(stream, value.m_matrix[row][column]);
        }
        Text(stream, row < 3 ? "]\n" : "]");
    }
    return stream;
}


NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(TextStream& stream, WebCore::ScrollingNodeType value) {
    using WebCore::ScrollingNodeType;
    switch (value) {
    case ScrollingNodeType::MainFrame: return Text(stream, "main-frame-scrolling");
    case ScrollingNodeType::Subframe: return Text(stream, "subframe-scrolling");
    case ScrollingNodeType::FrameHosting: return Text(stream, "frame-hosting");
    case ScrollingNodeType::Overflow: return Text(stream, "overflow-scrolling");
    case ScrollingNodeType::OverflowProxy: return Text(stream, "overflow-scroll-proxy");
    case ScrollingNodeType::Fixed: return Text(stream, "fixed");
    case ScrollingNodeType::Sticky: return Text(stream, "sticky");
    case ScrollingNodeType::Positioned: return Text(stream, "positioned");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(TextStream& stream, WebCore::ScrollingLayerPositionAction value) {
    using WebCore::ScrollingLayerPositionAction;
    switch (value) {
    case ScrollingLayerPositionAction::Set: return Text(stream, "set");
    case ScrollingLayerPositionAction::SetApproximate: return Text(stream, "set approximate");
    case ScrollingLayerPositionAction::Sync: return Text(stream, "sync");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(TextStream& stream, WebCore::ViewportRectStability value) {
    using WebCore::ViewportRectStability;
    switch (value) {
    case ViewportRectStability::Stable: return Text(stream, "stable");
    case ViewportRectStability::Unstable: return Text(stream, "unstable");
    case ViewportRectStability::ChangingObscuredInsetsInteractively: return Text(stream, "changing obscured insets interactively");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(TextStream& stream, WebCore::GraphicsLayerPaintingPhase value) {
    using WebCore::GraphicsLayerPaintingPhase;
    switch (value) {
    case GraphicsLayerPaintingPhase::Background: return Text(stream, "background");
    case GraphicsLayerPaintingPhase::Foreground: return Text(stream, "foreground");
    case GraphicsLayerPaintingPhase::Mask: return Text(stream, "mask");
    case GraphicsLayerPaintingPhase::ClipPath: return Text(stream, "clip-path");
    case GraphicsLayerPaintingPhase::OverflowContents: return Text(stream, "overflow-contents");
    case GraphicsLayerPaintingPhase::CompositedScroll: return Text(stream, "composited-scroll");
    case GraphicsLayerPaintingPhase::ChildClippingMask: return Text(stream, "child-clipping-mask");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(TextStream& stream, WebCore::ScrollPositioningBehavior value) {
    using WebCore::ScrollPositioningBehavior;
    switch (value) {
    case ScrollPositioningBehavior::None: return Text(stream, "none");
    case ScrollPositioningBehavior::Stationary: return Text(stream, "stationary");
    case ScrollPositioningBehavior::Moves: return Text(stream, "moves");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(TextStream& stream, const WebCore::GraphicsLayer::CustomAppearance& value) {
    using WebCore::GraphicsLayer;
    switch (value) {
    case GraphicsLayer::CustomAppearance::None: return Text(stream, "none");
    case GraphicsLayer::CustomAppearance::ScrollingOverhang: return Text(stream, "scrolling-overhang");
    case GraphicsLayer::CustomAppearance::ScrollingShadow: return Text(stream, "scrolling-shadow");
    }
    return stream;
}
}
