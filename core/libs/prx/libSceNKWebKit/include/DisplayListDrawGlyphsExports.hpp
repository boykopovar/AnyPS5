#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

class Font;

}

namespace NKWebKit::DisplayListAbi {

template<typename T, std::size_t inlineCapacity> struct InlineVector {
    T* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
    unsigned char inlineStorage[sizeof(T) * inlineCapacity];
};

using GlyphVector = InlineVector<unsigned short, 128>;
using GlyphAdvanceVector = InlineVector<WebCore::FloatSize, 128>;

static_assert(std::is_standard_layout_v<GlyphVector>);
static_assert(sizeof(GlyphVector) == 272);
static_assert(offsetof(GlyphVector, buffer) == 0);
static_assert(offsetof(GlyphVector, capacity) == 8);
static_assert(offsetof(GlyphVector, size) == 12);
static_assert(offsetof(GlyphVector, inlineStorage) == 16);
static_assert(std::is_standard_layout_v<GlyphAdvanceVector>);
static_assert(sizeof(GlyphAdvanceVector) == 1040);
static_assert(offsetof(GlyphAdvanceVector, buffer) == 0);
static_assert(offsetof(GlyphAdvanceVector, inlineStorage) == 16);

struct DrawGlyphs {
    Item item;
    OptionalFloatRect extent;
    const WebCore::Font* font;
    GlyphVector glyphs;
    GlyphAdvanceVector advances;
    WebCore::FloatRect bounds;
    WebCore::FloatPoint blockLocation;
    WebCore::FloatSize localAnchor;
    std::uint8_t smoothingMode;
};

static_assert(std::is_standard_layout_v<DrawGlyphs>);
static_assert(sizeof(DrawGlyphs) == 1400);
static_assert(offsetof(DrawGlyphs, font) == 40);
static_assert(offsetof(DrawGlyphs, glyphs) == 48);
static_assert(offsetof(DrawGlyphs, advances) == 320);
static_assert(offsetof(DrawGlyphs, bounds) == 1360);
static_assert(offsetof(DrawGlyphs, blockLocation) == 1376);
static_assert(offsetof(DrawGlyphs, localAnchor) == 1384);
static_assert(offsetof(DrawGlyphs, smoothingMode) == 1392);

}  // namespace NKWebKit::DisplayListAbi

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsC1ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
    NKWebKit::DisplayListAbi::DrawGlyphs* self, const WebCore::Font* font, NKWebKit::DisplayListAbi::GlyphVector* glyphs,
    NKWebKit::DisplayListAbi::GlyphAdvanceVector* advances, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor,
    int smoothingMode);
void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsC2ERKNS_4FontEON3WTF6VectorItLm128ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEONS6_INS_9FloatSizeELm128ES7_Lm16ES8_EERKNS_10FloatPointERKSB_NS_17FontSmoothingModeE(
    NKWebKit::DisplayListAbi::DrawGlyphs* self, const WebCore::Font* font, NKWebKit::DisplayListAbi::GlyphVector* glyphs,
    NKWebKit::DisplayListAbi::GlyphAdvanceVector* advances, const WebCore::FloatPoint* blockLocation, const WebCore::FloatSize* localAnchor,
    int smoothingMode);
void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD0Ev(NKWebKit::DisplayListAbi::DrawGlyphs* self);
void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD1Ev(NKWebKit::DisplayListAbi::DrawGlyphs* self);
void APS5_VABI _ZN7WebCore11DisplayList10DrawGlyphsD2Ev(NKWebKit::DisplayListAbi::DrawGlyphs* self);

}
