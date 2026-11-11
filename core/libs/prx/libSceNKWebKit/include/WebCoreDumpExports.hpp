#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include <cstddef>
#include <cstdint>

namespace WebCore {

struct FloatPoint3D {
    float m_x;
    float m_y;
    float m_z;
};

enum class CompositeOperator : std::uint8_t {
    Clear,
    Copy,
    SourceOver,
    SourceIn,
    SourceOut,
    SourceAtop,
    DestinationOver,
    DestinationIn,
    DestinationOut,
    DestinationAtop,
    XOR,
    PlusDarker,
    PlusLighter,
    Difference
};

enum class BlendMode : std::uint8_t {
    Normal = 1,
    Multiply,
    Screen,
    Darken,
    Lighten,
    Overlay,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
    PlusDarker,
    PlusLighter
};

enum class WindRule : bool { NonZero, EvenOdd };

enum class LineCap : std::uint8_t { Butt, Round, Square };

enum class LineJoin : std::uint8_t { Miter, Round, Bevel };

enum class ColorSpace : std::uint8_t {
    A98RGB,
    DisplayP3,
    ExtendedA98RGB,
    ExtendedDisplayP3,
    ExtendedLinearSRGB,
    ExtendedProPhotoRGB,
    ExtendedRec2020,
    ExtendedSRGB,
    HSL,
    HWB,
    LCH,
    Lab,
    LinearSRGB,
    OKLCH,
    OKLab,
    ProPhotoRGB,
    Rec2020,
    SRGB,
    XYZ_D50,
    XYZ_D65
};

enum class AlphaPremultiplication : std::uint8_t { Premultiplied, Unpremultiplied };

enum class ScrollType : bool { User, Programmatic };

enum class ScrollClamping : bool { Unclamped, Clamped };

enum class ScrollBehaviorForFixedElements : bool { StickToDocumentBounds, StickToViewportBounds };

enum class ScrollingNodeType : std::uint8_t {
    MainFrame,
    Subframe,
    FrameHosting,
    Overflow,
    OverflowProxy,
    Fixed,
    Sticky,
    Positioned
};

enum class ScrollingLayerPositionAction : std::uint8_t { Set, SetApproximate, Sync };

enum class ViewportRectStability : int { Stable, Unstable, ChangingObscuredInsetsInteractively };

enum class GraphicsLayerPaintingPhase : int {
    Background = 1 << 0,
    Foreground = 1 << 1,
    Mask = 1 << 2,
    ClipPath = 1 << 3,
    OverflowContents = 1 << 4,
    CompositedScroll = 1 << 5,
    ChildClippingMask = 1 << 6
};

enum class ScrollPositioningBehavior : std::uint8_t { None, Moves, Stationary };

struct GraphicsLayer {
    enum class CustomAppearance : std::uint8_t { None, ScrollingOverhang, ScrollingShadow };
};

struct ScrollAlignment {
    enum class Behavior : int {
        NoScroll,
        AlignCenter,
        AlignTop,
        AlignBottom,
        AlignLeft,
        AlignRight,
        AlignToClosestEdge
    };

    Behavior m_rectVisible;
    Behavior m_rectHidden;
    Behavior m_rectPartial;
};

static_assert(sizeof(FloatPoint3D) == 12);
static_assert(sizeof(ScrollAlignment) == 12);
static_assert(sizeof(WindRule) == 1);
static_assert(sizeof(ScrollType) == 1);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ColorSpace value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollType value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollClamping value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollAlignment::Behavior value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17CompositeOperatorE(NKWebKit::WtfAbi::TextStream& stream, WebCore::CompositeOperator value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(NKWebKit::WtfAbi::TextStream& stream, WebCore::AlphaPremultiplication value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollBehaviorForFixedElements value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(NKWebKit::WtfAbi::TextStream& stream, WebCore::LineCap value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8LineJoinE(NKWebKit::WtfAbi::TextStream& stream, WebCore::LineJoin value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(NKWebKit::WtfAbi::TextStream& stream, WebCore::WindRule value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_9BlendModeE(NKWebKit::WtfAbi::TextStream& stream, WebCore::BlendMode value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatPoint& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::LayoutRect& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutSizeE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::LayoutSize& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutUnitE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::LayoutUnit& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_11LayoutPointE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::LayoutPoint& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12FloatPoint3DE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatPoint3D& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::AffineTransform& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15ScrollAlignmentE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::ScrollAlignment& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::TransformationMatrix& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::IntRect& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::IntSize& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::IntPoint& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatRect& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatSize& value);

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollingNodeType value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollingLayerPositionAction value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ViewportRectStability value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(NKWebKit::WtfAbi::TextStream& stream, WebCore::GraphicsLayerPaintingPhase value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollPositioningBehavior value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::GraphicsLayer::CustomAppearance& value);
}
