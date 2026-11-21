#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::GraphicsContextAbi {

struct State {
    void* strokeGradient;
    void* strokePattern;
    void* fillGradient;
    void* fillPattern;
    WebCore::FloatSize shadowOffset;
    float strokeThickness;
    float shadowBlur;
    std::uint8_t textDrawingMode;
    unsigned char padding1[7];
    WebCore::Color strokeColor;
    WebCore::Color fillColor;
    WebCore::Color shadowColor;
    std::uint8_t strokeStyle;
    std::uint8_t fillRule;
    unsigned char padding2[2];
    float alpha;
    std::uint8_t compositeOperator;
    std::uint8_t blendMode;
    std::uint8_t imageInterpolationQuality;
    std::uint8_t flags;
};

struct Context {
    void* data;
    void* impl;
    State state;
    void* stackStorage;
    unsigned stackCapacity;
    unsigned stackSize;
    std::uint8_t paintInvalidationReasons;
    std::uint8_t padding1[3];
    std::uint32_t transparencyCount;
    bool contentfulPaintDetected;
    unsigned char padding2[7];
};

struct PlatformContext {
    void* cr;
};

struct ImagePaintingOptions {
    std::uint8_t compositeOperator;
    std::uint8_t blendMode;
    std::uint8_t decodingMode;
    unsigned char padding1;
    int orientation;
    std::uint8_t interpolationQuality;
    unsigned char padding2[3];
};

static_assert(std::is_standard_layout_v<State>);
static_assert(sizeof(State) == 96);
static_assert(alignof(State) == 8);
static_assert(offsetof(State, strokeGradient) == 0);
static_assert(offsetof(State, strokePattern) == 8);
static_assert(offsetof(State, fillGradient) == 16);
static_assert(offsetof(State, fillPattern) == 24);
static_assert(offsetof(State, shadowOffset) == 32);
static_assert(offsetof(State, strokeThickness) == 40);
static_assert(offsetof(State, shadowBlur) == 44);
static_assert(offsetof(State, textDrawingMode) == 48);
static_assert(offsetof(State, strokeColor) == 56);
static_assert(offsetof(State, fillColor) == 64);
static_assert(offsetof(State, shadowColor) == 72);
static_assert(offsetof(State, strokeStyle) == 80);
static_assert(offsetof(State, fillRule) == 81);
static_assert(offsetof(State, alpha) == 84);
static_assert(offsetof(State, compositeOperator) == 88);
static_assert(offsetof(State, blendMode) == 89);
static_assert(offsetof(State, imageInterpolationQuality) == 90);
static_assert(offsetof(State, flags) == 91);

static_assert(std::is_standard_layout_v<Context>);
static_assert(sizeof(Context) == 144);
static_assert(alignof(Context) == 8);
static_assert(offsetof(Context, data) == 0);
static_assert(offsetof(Context, impl) == 8);
static_assert(offsetof(Context, state) == 16);
static_assert(offsetof(Context, stackStorage) == 112);
static_assert(offsetof(Context, paintInvalidationReasons) == 128);
static_assert(offsetof(Context, transparencyCount) == 132);
static_assert(offsetof(Context, contentfulPaintDetected) == 136);

static_assert(sizeof(PlatformContext) == 8);
static_assert(offsetof(PlatformContext, cr) == 0);

static_assert(std::is_standard_layout_v<ImagePaintingOptions>);
static_assert(sizeof(ImagePaintingOptions) == 12);
static_assert(offsetof(ImagePaintingOptions, compositeOperator) == 0);
static_assert(offsetof(ImagePaintingOptions, blendMode) == 1);
static_assert(offsetof(ImagePaintingOptions, decodingMode) == 2);
static_assert(offsetof(ImagePaintingOptions, orientation) == 4);
static_assert(offsetof(ImagePaintingOptions, interpolationQuality) == 8);

}  // namespace NKWebKit::GraphicsContextAbi
