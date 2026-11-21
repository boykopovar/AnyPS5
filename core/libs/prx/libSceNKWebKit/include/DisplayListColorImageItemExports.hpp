#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::DisplayListAbi {

struct Color {
    std::uint64_t inlineColorAndFlags;
};

struct ExtendedColor {
    std::uint32_t refCount;
    float components[4];
    std::uint8_t colorSpace;
};

struct Image {
    const void* const* vptr;
    std::uint32_t refCount;
};

struct ImagePaintingOptions {
    std::uint8_t compositeOperator;
    std::uint8_t blendMode;
    std::uint8_t decodingMode;
    std::uint8_t padding0;
    int orientation;
    std::uint8_t interpolationQuality;
    std::uint8_t padding1[3];
};

static_assert(std::is_standard_layout_v<Color>);
static_assert(sizeof(Color) == 8);
static_assert(std::is_standard_layout_v<ExtendedColor>);
static_assert(offsetof(ExtendedColor, refCount) == 0);
static_assert(std::is_standard_layout_v<Image>);
static_assert(offsetof(Image, refCount) == 8);
static_assert(std::is_standard_layout_v<ImagePaintingOptions>);
static_assert(sizeof(ImagePaintingOptions) == 12);
static_assert(offsetof(ImagePaintingOptions, orientation) == 4);
static_assert(offsetof(ImagePaintingOptions, interpolationQuality) == 8);

struct FillRectWithColor {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    Color color;
};

static_assert(std::is_standard_layout_v<FillRectWithColor>);
static_assert(sizeof(FillRectWithColor) == 64);
static_assert(offsetof(FillRectWithColor, rect) == 36);
static_assert(offsetof(FillRectWithColor, color) == 56);

struct FillCompositedRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    Color color;
    std::uint8_t op;
    std::uint8_t blendMode;
};

static_assert(std::is_standard_layout_v<FillCompositedRect>);
static_assert(sizeof(FillCompositedRect) == 72);
static_assert(offsetof(FillCompositedRect, rect) == 36);
static_assert(offsetof(FillCompositedRect, color) == 56);
static_assert(offsetof(FillCompositedRect, op) == 64);
static_assert(offsetof(FillCompositedRect, blendMode) == 65);

struct FillRoundedRect {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRoundedRect rect;
    Color color;
    std::uint8_t blendMode;
};

static_assert(std::is_standard_layout_v<FillRoundedRect>);
static_assert(sizeof(FillRoundedRect) == 104);
static_assert(offsetof(FillRoundedRect, rect) == 36);
static_assert(offsetof(FillRoundedRect, color) == 88);
static_assert(offsetof(FillRoundedRect, blendMode) == 96);

struct FillRectWithRoundedHole {
    Item item;
    OptionalFloatRect extent;
    WebCore::FloatRect rect;
    WebCore::FloatRoundedRect roundedHoleRect;
    Color color;
};

static_assert(std::is_standard_layout_v<FillRectWithRoundedHole>);
static_assert(sizeof(FillRectWithRoundedHole) == 112);
static_assert(offsetof(FillRectWithRoundedHole, rect) == 36);
static_assert(offsetof(FillRectWithRoundedHole, roundedHoleRect) == 52);
static_assert(offsetof(FillRectWithRoundedHole, color) == 104);

struct DrawFocusRingRects {
    Item item;
    OptionalFloatRect extent;
    Vector<WebCore::FloatRect> rects;
    float width;
    float offset;
    Color color;
};

static_assert(std::is_standard_layout_v<DrawFocusRingRects>);
static_assert(sizeof(DrawFocusRingRects) == 72);
static_assert(offsetof(DrawFocusRingRects, rects) == 40);
static_assert(offsetof(DrawFocusRingRects, width) == 56);
static_assert(offsetof(DrawFocusRingRects, offset) == 60);
static_assert(offsetof(DrawFocusRingRects, color) == 64);

struct DrawImage {
    Item item;
    OptionalFloatRect extent;
    Image* image;
    WebCore::FloatRect destination;
    WebCore::FloatRect source;
    ImagePaintingOptions imagePaintingOptions;
};

static_assert(std::is_standard_layout_v<DrawImage>);
static_assert(sizeof(DrawImage) == 96);
static_assert(offsetof(DrawImage, image) == 40);
static_assert(offsetof(DrawImage, destination) == 48);
static_assert(offsetof(DrawImage, source) == 64);
static_assert(offsetof(DrawImage, imagePaintingOptions) == 80);

struct DrawTiledImage {
    Item item;
    OptionalFloatRect extent;
    Image* image;
    WebCore::FloatRect destination;
    WebCore::FloatPoint source;
    WebCore::FloatSize tileSize;
    WebCore::FloatSize spacing;
    ImagePaintingOptions imagePaintingOptions;
};

static_assert(std::is_standard_layout_v<DrawTiledImage>);
static_assert(sizeof(DrawTiledImage) == 104);
static_assert(offsetof(DrawTiledImage, image) == 40);
static_assert(offsetof(DrawTiledImage, destination) == 48);
static_assert(offsetof(DrawTiledImage, source) == 64);
static_assert(offsetof(DrawTiledImage, tileSize) == 72);
static_assert(offsetof(DrawTiledImage, spacing) == 80);
static_assert(offsetof(DrawTiledImage, imagePaintingOptions) == 88);

struct DrawTiledScaledImage {
    Item item;
    OptionalFloatRect extent;
    Image* image;
    WebCore::FloatRect destination;
    WebCore::FloatRect source;
    WebCore::FloatSize tileScaleFactor;
    int hRule;
    int vRule;
    ImagePaintingOptions imagePaintingOptions;
};

static_assert(std::is_standard_layout_v<DrawTiledScaledImage>);
static_assert(sizeof(DrawTiledScaledImage) == 112);
static_assert(offsetof(DrawTiledScaledImage, image) == 40);
static_assert(offsetof(DrawTiledScaledImage, destination) == 48);
static_assert(offsetof(DrawTiledScaledImage, source) == 64);
static_assert(offsetof(DrawTiledScaledImage, tileScaleFactor) == 80);
static_assert(offsetof(DrawTiledScaledImage, hRule) == 88);
static_assert(offsetof(DrawTiledScaledImage, vRule) == 92);
static_assert(offsetof(DrawTiledScaledImage, imagePaintingOptions) == 96);

struct DrawPattern {
    Item item;
    OptionalFloatRect extent;
    Image* image;
    WebCore::AffineTransform patternTransform;
    WebCore::FloatRect tileRect;
    WebCore::FloatRect destination;
    WebCore::FloatPoint phase;
    WebCore::FloatSize spacing;
    ImagePaintingOptions options;
};

static_assert(std::is_standard_layout_v<DrawPattern>);
static_assert(sizeof(DrawPattern) == 160);
static_assert(offsetof(DrawPattern, image) == 40);
static_assert(offsetof(DrawPattern, patternTransform) == 48);
static_assert(offsetof(DrawPattern, tileRect) == 96);
static_assert(offsetof(DrawPattern, destination) == 112);
static_assert(offsetof(DrawPattern, phase) == 128);
static_assert(offsetof(DrawPattern, spacing) == 136);
static_assert(offsetof(DrawPattern, options) == 144);

}  // namespace NKWebKit::DisplayListAbi

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorC1ERKNS_9FloatRectERKNS_5ColorE(NKWebKit::DisplayListAbi::FillRectWithColor* self, const WebCore::FloatRect* rect, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorC2ERKNS_9FloatRectERKNS_5ColorE(NKWebKit::DisplayListAbi::FillRectWithColor* self, const WebCore::FloatRect* rect, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD0Ev(NKWebKit::DisplayListAbi::FillRectWithColor* self);
void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD1Ev(NKWebKit::DisplayListAbi::FillRectWithColor* self);
void APS5_VABI _ZN7WebCore11DisplayList17FillRectWithColorD2Ev(NKWebKit::DisplayListAbi::FillRectWithColor* self);

void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectC1ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(NKWebKit::DisplayListAbi::FillCompositedRect* self, const WebCore::FloatRect* rect, const NKWebKit::DisplayListAbi::Color* color, std::uint8_t op, std::uint8_t blendMode);
void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectC2ERKNS_9FloatRectERKNS_5ColorENS_17CompositeOperatorENS_9BlendModeE(NKWebKit::DisplayListAbi::FillCompositedRect* self, const WebCore::FloatRect* rect, const NKWebKit::DisplayListAbi::Color* color, std::uint8_t op, std::uint8_t blendMode);
void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD0Ev(NKWebKit::DisplayListAbi::FillCompositedRect* self);
void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD1Ev(NKWebKit::DisplayListAbi::FillCompositedRect* self);
void APS5_VABI _ZN7WebCore11DisplayList18FillCompositedRectD2Ev(NKWebKit::DisplayListAbi::FillCompositedRect* self);

void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectC1ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(NKWebKit::DisplayListAbi::FillRoundedRect* self, const WebCore::FloatRoundedRect* rect, const NKWebKit::DisplayListAbi::Color* color, std::uint8_t blendMode);
void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectC2ERKNS_16FloatRoundedRectERKNS_5ColorENS_9BlendModeE(NKWebKit::DisplayListAbi::FillRoundedRect* self, const WebCore::FloatRoundedRect* rect, const NKWebKit::DisplayListAbi::Color* color, std::uint8_t blendMode);
void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD0Ev(NKWebKit::DisplayListAbi::FillRoundedRect* self);
void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD1Ev(NKWebKit::DisplayListAbi::FillRoundedRect* self);
void APS5_VABI _ZN7WebCore11DisplayList15FillRoundedRectD2Ev(NKWebKit::DisplayListAbi::FillRoundedRect* self);

void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC1ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(NKWebKit::DisplayListAbi::FillRectWithRoundedHole* self, const WebCore::FloatRect* rect, const WebCore::FloatRoundedRect* roundedHoleRect, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleC2ERKNS_9FloatRectERKNS_16FloatRoundedRectERKNS_5ColorE(NKWebKit::DisplayListAbi::FillRectWithRoundedHole* self, const WebCore::FloatRect* rect, const WebCore::FloatRoundedRect* roundedHoleRect, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD0Ev(NKWebKit::DisplayListAbi::FillRectWithRoundedHole* self);
void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD1Ev(NKWebKit::DisplayListAbi::FillRectWithRoundedHole* self);
void APS5_VABI _ZN7WebCore11DisplayList23FillRectWithRoundedHoleD2Ev(NKWebKit::DisplayListAbi::FillRectWithRoundedHole* self);

void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsC1ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(NKWebKit::DisplayListAbi::DrawFocusRingRects* self, const NKWebKit::DisplayListAbi::Vector<WebCore::FloatRect>* rects, float width, float offset, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsC2ERKN3WTF6VectorINS_9FloatRectELm0ENS2_15CrashOnOverflowELm16ENS2_10FastMallocEEEffRKNS_5ColorE(NKWebKit::DisplayListAbi::DrawFocusRingRects* self, const NKWebKit::DisplayListAbi::Vector<WebCore::FloatRect>* rects, float width, float offset, const NKWebKit::DisplayListAbi::Color* color);
void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD0Ev(NKWebKit::DisplayListAbi::DrawFocusRingRects* self);
void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD1Ev(NKWebKit::DisplayListAbi::DrawFocusRingRects* self);
void APS5_VABI _ZN7WebCore11DisplayList18DrawFocusRingRectsD2Ev(NKWebKit::DisplayListAbi::DrawFocusRingRects* self);

void APS5_VABI _ZN7WebCore11DisplayList9DrawImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList9DrawImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD0Ev(NKWebKit::DisplayListAbi::DrawImage* self);
void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD1Ev(NKWebKit::DisplayListAbi::DrawImage* self);
void APS5_VABI _ZN7WebCore11DisplayList9DrawImageD2Ev(NKWebKit::DisplayListAbi::DrawImage* self);

void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageC1ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawTiledImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatPoint* source, const WebCore::FloatSize* tileSize, const WebCore::FloatSize* spacing, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageC2ERNS_5ImageERKNS_9FloatRectERKNS_10FloatPointERKNS_9FloatSizeESC_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawTiledImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatPoint* source, const WebCore::FloatSize* tileSize, const WebCore::FloatSize* spacing, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD0Ev(NKWebKit::DisplayListAbi::DrawTiledImage* self);
void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD1Ev(NKWebKit::DisplayListAbi::DrawTiledImage* self);
void APS5_VABI _ZN7WebCore11DisplayList14DrawTiledImageD2Ev(NKWebKit::DisplayListAbi::DrawTiledImage* self);

void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawTiledScaledImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const WebCore::FloatSize* tileScaleFactor, int hRule, int vRule, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_9FloatSizeENS2_8TileRuleESA_RKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawTiledScaledImage* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const WebCore::FloatSize* tileScaleFactor, int hRule, int vRule, const NKWebKit::DisplayListAbi::ImagePaintingOptions* imagePaintingOptions);
void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD0Ev(NKWebKit::DisplayListAbi::DrawTiledScaledImage* self);
void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD1Ev(NKWebKit::DisplayListAbi::DrawTiledScaledImage* self);
void APS5_VABI _ZN7WebCore11DisplayList20DrawTiledScaledImageD2Ev(NKWebKit::DisplayListAbi::DrawTiledScaledImage* self);

void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternC1ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawPattern* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* tileRect, const WebCore::AffineTransform* patternTransform, const WebCore::FloatPoint* phase, const WebCore::FloatSize* spacing, const NKWebKit::DisplayListAbi::ImagePaintingOptions* options);
void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternC2ERNS_5ImageERKNS_9FloatRectES6_RKNS_15AffineTransformERKNS_10FloatPointERKNS_9FloatSizeERKNS_20ImagePaintingOptionsE(NKWebKit::DisplayListAbi::DrawPattern* self, NKWebKit::DisplayListAbi::Image* image, const WebCore::FloatRect* destination, const WebCore::FloatRect* tileRect, const WebCore::AffineTransform* patternTransform, const WebCore::FloatPoint* phase, const WebCore::FloatSize* spacing, const NKWebKit::DisplayListAbi::ImagePaintingOptions* options);
void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD0Ev(NKWebKit::DisplayListAbi::DrawPattern* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD1Ev(NKWebKit::DisplayListAbi::DrawPattern* self);
void APS5_VABI _ZN7WebCore11DisplayList11DrawPatternD2Ev(NKWebKit::DisplayListAbi::DrawPattern* self);

}
