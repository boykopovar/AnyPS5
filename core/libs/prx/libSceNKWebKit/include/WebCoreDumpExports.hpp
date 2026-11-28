#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include <cstddef>
#include <cstdint>

namespace WebCore {

struct FloatPoint3D {
    float m_x;
    float m_y;
    float m_z;
};

struct AbsolutePositionConstraints {
    FloatSize m_alignmentOffset;
    FloatPoint m_layerPositionAtLastLayout;
};

struct FixedPositionViewportConstraints {
    const void* m_vtable;
    FloatSize m_alignmentOffset;
    std::uint32_t m_anchorEdges;
    FloatRect m_viewportRectAtLastLayout;
    FloatPoint m_layerPositionAtLastLayout;
};

static_assert(sizeof(AbsolutePositionConstraints) == 16);
static_assert(offsetof(AbsolutePositionConstraints, m_layerPositionAtLastLayout) == 8);
static_assert(sizeof(FixedPositionViewportConstraints) == 48);
static_assert(alignof(FixedPositionViewportConstraints) == 8);
static_assert(offsetof(FixedPositionViewportConstraints, m_alignmentOffset) == 8);
static_assert(offsetof(FixedPositionViewportConstraints, m_anchorEdges) == 16);
static_assert(offsetof(FixedPositionViewportConstraints, m_viewportRectAtLastLayout) == 20);
static_assert(offsetof(FixedPositionViewportConstraints, m_layerPositionAtLastLayout) == 36);

enum class TouchAction : std::uint8_t {
    Auto = 1 << 0,
    None = 1 << 1,
    Manipulation = 1 << 2,
    PanX = 1 << 3,
    PanY = 1 << 4,
    PinchZoom = 1 << 5,
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

enum class ScrollElasticity : std::uint8_t { Automatic, None, Allowed };

enum class ScrollbarMode : std::uint8_t { Auto, AlwaysOff, AlwaysOn };

enum class NativeScrollbarVisibility : std::uint8_t { Visible, HiddenByStyle, ReplacedByCustomScrollbar };

enum class OverscrollBehavior : std::uint8_t { Auto, Contain, None };

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity{ScrollElasticity::None};
    ScrollElasticity verticalScrollElasticity{ScrollElasticity::None};
    ScrollbarMode horizontalScrollbarMode{ScrollbarMode::Auto};
    ScrollbarMode verticalScrollbarMode{ScrollbarMode::Auto};
    OverscrollBehavior horizontalOverscrollBehavior{OverscrollBehavior::Auto};
    OverscrollBehavior verticalOverscrollBehavior{OverscrollBehavior::Auto};
    bool allowsHorizontalScrolling{false};
    bool allowsVerticalScrolling{false};
    NativeScrollbarVisibility horizontalNativeScrollbarVisibility{NativeScrollbarVisibility::Visible};
    NativeScrollbarVisibility verticalNativeScrollbarVisibility{NativeScrollbarVisibility::Visible};
    bool useDarkAppearanceForScrollbars{false};
};

struct Position {
    void* m_anchorNode{nullptr};
    unsigned m_offset{0};
    unsigned m_anchorTypeBits{0};
};

struct VisiblePosition {
    Position m_deepPosition;
    std::uint8_t m_affinity{0};
};

struct VisibleSelection {
    Position m_anchor;
    Position m_focus;
    Position m_base;
    Position m_extent;
    Position m_start;
    Position m_end;
    std::uint8_t m_affinity{0};
    std::uint8_t m_type{0};
    bool m_anchorIsFirst : 1 {false};
    bool m_isDirectional : 1 {false};
};

static_assert(sizeof(FloatPoint3D) == 12);
static_assert(sizeof(ScrollAlignment) == 12);
static_assert(sizeof(WindRule) == 1);
static_assert(sizeof(ScrollType) == 1);
static_assert(sizeof(ScrollableAreaParameters) == 11);
static_assert(sizeof(Position) == 16);
static_assert(offsetof(Position, m_offset) == 8);
static_assert(offsetof(Position, m_anchorTypeBits) == 12);
static_assert(sizeof(VisiblePosition) == 24);
static_assert(offsetof(VisiblePosition, m_affinity) == 16);
static_assert(offsetof(VisibleSelection, m_base) == 32);
static_assert(offsetof(VisibleSelection, m_extent) == 48);
static_assert(offsetof(VisibleSelection, m_start) == 64);
static_assert(offsetof(VisibleSelection, m_end) == 80);
static_assert(sizeof(VisibleSelection) == 104);

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
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS0_9OptionSetINS_13ActivityState4FlagEEE(NKWebKit::WtfAbi::TextStream& stream, std::uint32_t flags);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(NKWebKit::WtfAbi::TextStream& stream, WebCore::TouchAction touchAction);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::Region& region);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::AbsolutePositionConstraints& constraints);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FixedPositionViewportConstraints& constraints);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::IntPoint& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatRect& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FloatSize& value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::VelocityData& value);

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollingNodeType value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollingLayerPositionAction value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ViewportRectStability value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(NKWebKit::WtfAbi::TextStream& stream, WebCore::GraphicsLayerPaintingPhase value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollPositioningBehavior value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::GraphicsLayer::CustomAppearance& value);

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(NKWebKit::WtfAbi::TextStream& stream, const NKWebKit::WtfAbi::Vector<std::uint64_t>& vector);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollableAreaParameters value);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15VisiblePositionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::VisiblePosition& visiblePosition);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16VisibleSelectionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::VisibleSelection& selection);
}
