// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTINTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTINTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "prx/libSceFont/include/FontDriver.hpp"

namespace Font {

constexpr std::uint8_t STYLE_FRAME_FLAG_SCALE = 0x01;
constexpr std::uint8_t STYLE_FRAME_FLAG_SLANT = 0x02;
constexpr std::uint8_t STYLE_FRAME_FLAG_WEIGHT = 0x04;
constexpr float POINTS_PER_INCH = 72.0f;

struct FontState {
    std::shared_ptr<const std::vector<unsigned char>> faceData;
    FT_Face face = nullptr;
    float scaleW = 16.0f;
    float scaleH = 16.0f;

    FontState() = default;
    FontState(const FontState&) = delete;
    FontState& operator=(const FontState&) = delete;
    ~FontState();
};

struct GeneratedGlyph {
    FontGlyphOpaque glyph{};
    FontGlyphMetrics metrics{};
    FontGlyphMetricsHorizontal metricsHorizontal{};
    FontGlyphMetricsHorizontalX metricsHorizontalX{};
    FontGlyphMetricsHorizontalAdvance metricsHorizontalAdvance{};
    std::uint32_t codepoint = 0;
    std::uint32_t glyphIndex = 0;
    std::uint32_t faceIndex = 0;
    FontHandle owner = nullptr;
    std::shared_ptr<const std::vector<unsigned char>> faceData;
    FontGlyphOutline outline{};
    std::vector<FontGlyphOutlinePoint> outlinePoints;
    std::vector<std::uint8_t> outlineTags;
    std::vector<std::uint16_t> outlineContours;
    bool unsupportedCreationEffects = false;
    bool metricsInitialized = false;
    bool outlineInitialized = false;
};

struct GlyphRenderState {
    std::shared_ptr<const std::vector<unsigned char>> faceData;
    std::uint32_t glyphIndex = 0;
    std::uint32_t faceIndex = 0;
    float scaleW = 0.0f;
    float scaleH = 0.0f;
    std::uint16_t flags = 0;
    std::uint8_t glyphForm = 0;
    std::uint8_t metricsForm = 0;
    bool unsupportedCreationEffects = false;
};

struct SystemFontFile {
    std::filesystem::path path;
    std::uint32_t subFontIndex = 0;
};

struct RenderSurfaceSystemUse {
    FontStyleFrame* styleframe;
    float catchedScale;
};

void Backoff();
std::uint32_t AcquireWordLock(std::uint32_t& word);
FontHandleNative* GetNativeFont(FontHandle handle);
bool AcquireLibraryLock(FontLibNative* library, std::uint32_t& previous);
void ReleaseLibraryLock(FontLibNative* library, std::uint32_t previous);
bool AcquireFontLock(FontHandleNative* font, std::uint32_t& previous);
void ReleaseFontLock(FontHandleNative* font, std::uint32_t previous);
bool AcquireCachedStyleLock(FontHandleNative* font, std::uint32_t& previous);
void ReleaseCachedStyleLock(FontHandleNative* font, std::uint32_t previous);
void LinkFontToLibrary(FontLibNative* library, FontHandle handle);
void UnlinkFontFromLibrary(FontLibNative* library, FontHandle handle);

std::uint32_t* EntryLockWord(FontCtxEntry* entry, std::uint32_t modeLow);
void** EntryObjectSlot(FontCtxEntry* entry, std::uint32_t modeLow);
FontCtxEntry* AcquireFontCtxEntry(void* ctx, std::uint32_t index, std::uint32_t modeLow, FontObj** outObject, std::uint32_t* outLockWord);
void ReleaseFontCtxEntryLock(FontCtxEntry* entry, std::uint32_t modeLow, std::uint32_t lockWord);
FontObj* FindSubFont(FontObj* head, std::uint32_t subFontIndex);
void* FontContext(const FontLibNative* library, const FontHandleNative* font);
void ReleaseFontObjectsForHandle(FontHandleNative* font);

FontState* TryGetState(FontHandle handle);
FontState& ResetState(FontHandle handle);
void RemoveState(FontHandle handle);
void LoadStateFace(FontState& state, std::shared_ptr<const std::vector<unsigned char>> bytes, std::uint32_t subFontIndex);

bool IsSystemFontSet(std::uint32_t fontSetType);
std::vector<SystemFontFile> SystemFontCandidates(std::uint32_t fontSetType);
std::filesystem::path SystemFontDirectory();
std::optional<SystemFontFile> FindSystemFontFile(std::uint32_t fontSetType);

void TrackGeneratedGlyph(FontGlyph glyph);
bool ForgetGeneratedGlyph(FontGlyph glyph);
GeneratedGlyph* TryGetGeneratedGlyph(FontGlyph glyph);
bool GetGlyphRenderState(FontGlyph glyph, GlyphRenderState& state);
void PopulateGlyphMetricVariants(GeneratedGlyph& glyph);
void BuildBoundingOutline(GeneratedGlyph& glyph);
bool BuildTrueOutline(GeneratedGlyph& glyph);

int StyleStateSetScalePixel(StyleStateBlock* style, float w, float h);
int StyleStateSetScalePoint(StyleStateBlock* style, float w, float h);
int StyleStateGetScalePixel(const StyleStateBlock* style, float* w, float* h);
int StyleStateGetScalePoint(const StyleStateBlock* style, float* w, float* h);
int StyleStateSetDpi(StyleStateBlock* style, std::uint32_t hDpi, std::uint32_t vDpi);
int StyleStateSetSlantRatio(StyleStateBlock* style, float slantRatio);
int StyleStateGetSlantRatio(const StyleStateBlock* style, float* slantRatio);
int StyleStateSetWeightScale(StyleStateBlock* style, float weightXScale, float weightYScale);
int StyleStateGetWeightScale(const StyleStateBlock* style, float* weightXScale, float* weightYScale, std::uint32_t* mode);
bool ValidStyleFrame(const FontStyleFrame* frame);

std::uint8_t CachedStyleCacheFlags(const CachedStyle& style);
void CachedStyleSetCacheFlags(CachedStyle& style, std::uint8_t flags);
void CachedStyleSetDirectionWord(CachedStyle& style, std::uint16_t word);
float CachedStyleGetScalar(const CachedStyle& style);
void CachedStyleSetScalar(CachedStyle& style, float value);

void ClearRenderOutputs(FontGlyphMetrics* metrics, FontRenderOutput* result);
int ComputeHorizontalLayout(FontHandle handle, const StyleStateBlock* style, std::uint8_t* outWords);
int ComputeVerticalLayout(FontHandle handle, const StyleStateBlock* style, std::uint8_t* outWords);
int GetCharGlyphMetrics(FontHandle handle, std::uint32_t code, FontGlyphMetrics* metrics, bool useCachedStyle);
int RenderCharGlyphImageCore(FontHandle handle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result);

}

#endif
