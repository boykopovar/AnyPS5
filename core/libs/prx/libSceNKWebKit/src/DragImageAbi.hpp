#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfRefPtr.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

enum class RotationDirection : bool {
    Counterclockwise,
    Clockwise,
};

}  // namespace WebCore

namespace NKWebKit::WebCoreAbi {

struct Image;
struct PathImpl;

enum class TextIndicatorPresentationTransition : std::uint8_t {
    None = 0,
    Bounce = 1,
    BounceAndCrossfade = 2,
    FadeIn = 3,
};

enum class TextIndicatorOption : std::uint16_t {
    RespectTextColor = 1 << 0,
    PaintBackgrounds = 1 << 1,
    PaintAllContent = 1 << 2,
    IncludeSnapshotWithSelectionHighlight = 1 << 3,
    TightlyFitContent = 1 << 4,
    UseBoundingRectAndPaintAllContentForComplexRanges = 1 << 5,
    IncludeMarginIfRangeMatchesSelection = 1 << 6,
    ExpandClipBeyondVisibleRect = 1 << 7,
    DoNotClipToVisibleRect = 1 << 8,
    IncludeSnapshotOfAllVisibleContentWithoutSelection = 1 << 9,
    UseSelectionRectForSizing = 1 << 10,
    ComputeEstimatedBackgroundColor = 1 << 11,
    UseUserSelectAllCommonAncestor = 1 << 12,
};

struct OptionSetTextIndicatorOption {
    std::uint16_t storage{0};
};

struct PathMoveTo {
    WebCore::FloatPoint point;
};

struct PathLineTo {
    WebCore::FloatPoint point;
};

struct PathQuadCurveTo {
    WebCore::FloatPoint controlPoint;
    WebCore::FloatPoint endPoint;
};

struct PathBezierCurveTo {
    WebCore::FloatPoint controlPoint1;
    WebCore::FloatPoint controlPoint2;
    WebCore::FloatPoint endPoint;
};

struct PathArcTo {
    WebCore::FloatPoint controlPoint1;
    WebCore::FloatPoint controlPoint2;
    float radius;
};

struct PathArc {
    WebCore::FloatPoint center;
    float radius;
    float startAngle;
    float endAngle;
    WebCore::RotationDirection direction;
};

struct PathEllipse {
    WebCore::FloatPoint center;
    float radiusX;
    float radiusY;
    float rotation;
    float startAngle;
    float endAngle;
    WebCore::RotationDirection direction;
};

struct PathEllipseInRect {
    WebCore::FloatRect rect;
};

struct PathRect {
    WebCore::FloatRect rect;
};

struct PathRoundedRect {
    enum class Strategy : std::uint8_t {
        PreferNative,
        PreferBezier,
    };

    WebCore::FloatRoundedRect roundedRect;
    Strategy strategy;
};

struct PathDataLine {
    WebCore::FloatPoint start;
    WebCore::FloatPoint end;
};

struct PathDataQuadCurve {
    WebCore::FloatPoint start;
    WebCore::FloatPoint controlPoint;
    WebCore::FloatPoint endPoint;
};

struct PathDataBezierCurve {
    WebCore::FloatPoint start;
    WebCore::FloatPoint controlPoint1;
    WebCore::FloatPoint controlPoint2;
    WebCore::FloatPoint endPoint;
};

struct PathDataArc {
    WebCore::FloatPoint start;
    WebCore::FloatPoint controlPoint1;
    WebCore::FloatPoint controlPoint2;
    float radius;
};

struct PathCloseSubpath {
    std::uint8_t empty{0};
};

struct PathSegment {
    union {
        std::uint8_t empty{0};
        PathMoveTo moveTo;
        PathLineTo lineTo;
        PathQuadCurveTo quadCurveTo;
        PathBezierCurveTo bezierCurveTo;
        PathArcTo arcTo;
        PathArc arc;
        PathEllipse ellipse;
        PathEllipseInRect ellipseInRect;
        PathRect rect;
        PathRoundedRect roundedRect;
        PathDataLine dataLine;
        PathDataQuadCurve dataQuadCurve;
        PathDataBezierCurve dataBezierCurve;
        PathDataArc dataArc;
        PathCloseSubpath closeSubpath;
    };
    std::uint8_t index{0};
    std::uint8_t padding[3];
};

struct Path {
    union {
        std::uint8_t empty{0};
        PathSegment segment;
        PathImpl* impl;
    };
    std::uint8_t index{0};
    std::uint8_t padding[7];
};

struct TextIndicatorData {
    WebCore::FloatRect selectionRectInRootViewCoordinates;
    WebCore::FloatRect textBoundingRectInRootViewCoordinates;
    WebCore::FloatRect contentImageWithoutSelectionRectInRootViewCoordinates;
    WtfAbi::Vector<WebCore::FloatRect> textRectsInBoundingRectCoordinates;
    float contentImageScaleFactor{1.0f};
    WtfAbi::RefPtr<Image> contentImageWithHighlight;
    WtfAbi::RefPtr<Image> contentImageWithoutSelection;
    WtfAbi::RefPtr<Image> contentImage;
    WebCore::Color estimatedBackgroundColor;
    TextIndicatorPresentationTransition presentationTransition{TextIndicatorPresentationTransition::None};
    OptionSetTextIndicatorOption options;
};

template<typename T>
struct Optional {
    T value;
    bool engaged{false};
};

struct DragImage {
    WtfAbi::RefPtr<CairoSurface> m_dragImageRef;
    Optional<TextIndicatorData> m_indicatorData;
    Optional<Path> m_visiblePath;
};

static_assert(sizeof(PathMoveTo) == 8);
static_assert(sizeof(PathArcTo) == 20);
static_assert(sizeof(PathArc) == 24);
static_assert(sizeof(PathEllipse) == 32);
static_assert(sizeof(PathRoundedRect) == 52);
static_assert(sizeof(PathDataArc) == 28);
static_assert(sizeof(PathCloseSubpath) == 1);
static_assert(sizeof(PathSegment) == 56);
static_assert(offsetof(PathSegment, index) == 52);
static_assert(sizeof(Path) == 64);
static_assert(offsetof(Path, index) == 56);
static_assert(sizeof(TextIndicatorData) == 112);
static_assert(offsetof(TextIndicatorData, textRectsInBoundingRectCoordinates) == 48);
static_assert(offsetof(TextIndicatorData, contentImageScaleFactor) == 64);
static_assert(offsetof(TextIndicatorData, contentImageWithHighlight) == 72);
static_assert(offsetof(TextIndicatorData, contentImageWithoutSelection) == 80);
static_assert(offsetof(TextIndicatorData, contentImage) == 88);
static_assert(offsetof(TextIndicatorData, estimatedBackgroundColor) == 96);
static_assert(offsetof(TextIndicatorData, presentationTransition) == 104);
static_assert(offsetof(TextIndicatorData, options) == 106);
static_assert(sizeof(Optional<TextIndicatorData>) == 120);
static_assert(offsetof(Optional<TextIndicatorData>, engaged) == 112);
static_assert(sizeof(Optional<Path>) == 72);
static_assert(offsetof(Optional<Path>, engaged) == 64);
static_assert(sizeof(DragImage) == 200);
static_assert(offsetof(DragImage, m_dragImageRef) == 0);
static_assert(offsetof(DragImage, m_indicatorData) == 8);
static_assert(offsetof(DragImage, m_visiblePath) == 128);
static_assert(std::is_standard_layout_v<DragImage>);

}  // namespace NKWebKit::WebCoreAbi
