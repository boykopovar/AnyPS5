#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::DisplayListNativeImageAbi {

struct ImagePaintingOptions {
    std::uint8_t compositeOperator;
    std::uint8_t blendMode;
    std::uint8_t decodingMode;
    unsigned char padding1;
    int orientation;
    std::uint8_t interpolationQuality;
    unsigned char padding2[3];
};

struct DrawNativeImage {
    DisplayListAbi::DrawingItem drawingItem;
    WebCore::FloatSize imageSize;
    WebCore::FloatRect destinationRect;
    WebCore::FloatRect sourceRect;
    ImagePaintingOptions options;
    unsigned char padding[4];
};

static_assert(std::is_standard_layout_v<ImagePaintingOptions>);
static_assert(sizeof(ImagePaintingOptions) == 12);
static_assert(offsetof(ImagePaintingOptions, compositeOperator) == 0);
static_assert(offsetof(ImagePaintingOptions, blendMode) == 1);
static_assert(offsetof(ImagePaintingOptions, decodingMode) == 2);
static_assert(offsetof(ImagePaintingOptions, orientation) == 4);
static_assert(offsetof(ImagePaintingOptions, interpolationQuality) == 8);

static_assert(std::is_standard_layout_v<DrawNativeImage>);
static_assert(sizeof(DrawNativeImage) == 96);
static_assert(alignof(DrawNativeImage) == 8);
static_assert(offsetof(DrawNativeImage, drawingItem) == 0);
static_assert(offsetof(DrawNativeImage, imageSize) == 40);
static_assert(offsetof(DrawNativeImage, destinationRect) == 48);
static_assert(offsetof(DrawNativeImage, sourceRect) == 64);
static_assert(offsetof(DrawNativeImage, options) == 80);

}
