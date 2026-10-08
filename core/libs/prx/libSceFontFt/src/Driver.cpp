// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H

#include "prx/libSceFont/include/FontFreeType.hpp"
#include "prx/libSceFontFt/include/FontFtDriver.hpp"

namespace {

using namespace Font;

struct FtLibraryContext {
    void* allocCtx;
    const FontMemoryInterface* iface;
    FT_Memory memory;
    FT_Library library;
    FT_Memory hostMemory;
    FT_Library hostLibrary;
};

void* FtAlloc(FT_Memory memory, long size) {
    if (!memory || size <= 0) return nullptr;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface || !ctx->iface->alloc) return nullptr;
    return ctx->iface->alloc(ctx->allocCtx, static_cast<std::uint32_t>(size));
}

void FtFree(FT_Memory memory, void* block) {
    if (!memory || !block) return;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface || !ctx->iface->dealloc) return;
    ctx->iface->dealloc(ctx->allocCtx, block);
}

void* FtRealloc(FT_Memory memory, long currentSize, long newSize, void* block) {
    if (!memory) return nullptr;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface) return nullptr;
    if (ctx->iface->realloc_fn) return ctx->iface->realloc_fn(ctx->allocCtx, block, static_cast<std::uint32_t>(newSize));
    if (newSize <= 0) {
        FtFree(memory, block);
        return nullptr;
    }
    void* result = FtAlloc(memory, newSize);
    if (!result) return nullptr;
    if (block && currentSize > 0) std::memcpy(result, block, static_cast<std::size_t>(std::min(currentSize, newSize)));
    FtFree(memory, block);
    return result;
}

void* HostAlloc(FT_Memory, long size) {
    return size > 0 ? std::malloc(static_cast<std::size_t>(size)) : nullptr;
}

void HostFree(FT_Memory, void* block) {
    std::free(block);
}

void* HostRealloc(FT_Memory, long, long newSize, void* block) {
    if (newSize <= 0) {
        std::free(block);
        return nullptr;
    }
    return std::realloc(block, static_cast<std::size_t>(newSize));
}

FT_Library SystemFontLibrary(FtLibraryContext* ctx) {
    if (ctx->hostLibrary) return ctx->hostLibrary;
    auto* hostMemory = static_cast<FT_Memory>(ctx->iface->alloc(ctx->allocCtx, sizeof(FT_MemoryRec_)));
    if (!hostMemory) return nullptr;
    std::memset(hostMemory, 0, sizeof(*hostMemory));
    hostMemory->user = ctx;
    hostMemory->alloc = &HostAlloc;
    hostMemory->free = &HostFree;
    hostMemory->realloc = &HostRealloc;
    FT_Library hostLibrary = nullptr;
    if (FT_New_Library(hostMemory, &hostLibrary) != 0 || !hostLibrary) {
        ctx->iface->dealloc(ctx->allocCtx, hostMemory);
        return nullptr;
    }
    FT_Add_Default_Modules(hostLibrary);
    ctx->hostMemory = hostMemory;
    ctx->hostLibrary = hostLibrary;
    return hostLibrary;
}

void WriteU16(std::uint8_t* out, std::size_t offset, std::uint16_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WriteU32(std::uint8_t* out, std::size_t offset, std::uint32_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WriteS32(std::uint8_t* out, std::size_t offset, std::int32_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WritePointer(std::uint8_t* out, std::size_t offset, const void* value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

float FromF26Dot6(std::int64_t value) {
    return static_cast<float>(value) * ONE_OVER_64;
}

FT_Face FaceOf(const FontObj* obj) {
    return static_cast<FT_Face>(obj->ft_face);
}

std::uint32_t APS5_VABI LibraryGetPixelResolution() {
    return 0x40;
}

int APS5_VABI LibraryInit(const FontMemory* memory, FontLibNative* library) {
    if (!memory || !library) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!memory->iface || !memory->iface->alloc || !memory->iface->dealloc) return SCE_FONT_ERROR_INVALID_MEMORY;
    const auto allocFn = memory->iface->alloc;
    const auto freeFn = memory->iface->dealloc;
    void* allocCtx = memory->mspace_handle;
    auto* ctx = static_cast<FtLibraryContext*>(allocFn(allocCtx, sizeof(FtLibraryContext)));
    if (!ctx) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    std::memset(ctx, 0, sizeof(FtLibraryContext));
    ctx->allocCtx = allocCtx;
    ctx->iface = memory->iface;
    auto* ftMemory = static_cast<FT_Memory>(allocFn(allocCtx, sizeof(FT_MemoryRec_)));
    if (!ftMemory) {
        freeFn(allocCtx, ctx);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    std::memset(ftMemory, 0, sizeof(*ftMemory));
    ftMemory->user = ctx;
    ftMemory->alloc = &FtAlloc;
    ftMemory->free = &FtFree;
    ftMemory->realloc = &FtRealloc;
    ctx->memory = ftMemory;
    FT_Library ftLibrary = nullptr;
    if (FT_New_Library(ftMemory, &ftLibrary) != 0 || !ftLibrary) {
        freeFn(allocCtx, ftMemory);
        freeFn(allocCtx, ctx);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    FT_Add_Default_Modules(ftLibrary);
    ctx->library = ftLibrary;
    library->fontset_registry = ctx;
    library->flags = 0x60000000;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryTerm(FontLibNative* library) {
    if (!library) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto freeFn = library->iface ? library->iface->dealloc : nullptr;
    if (!freeFn) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* ctx = static_cast<FtLibraryContext*>(library->fontset_registry);
    if (!ctx) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (ctx->library) {
        FT_Done_Library(ctx->library);
        ctx->library = nullptr;
    }
    if (ctx->hostLibrary) {
        FT_Done_Library(ctx->hostLibrary);
        ctx->hostLibrary = nullptr;
    }
    if (ctx->hostMemory) {
        freeFn(library->alloc_ctx, ctx->hostMemory);
        ctx->hostMemory = nullptr;
    }
    if (ctx->memory) {
        freeFn(library->alloc_ctx, ctx->memory);
        ctx->memory = nullptr;
    }
    freeFn(library->alloc_ctx, ctx);
    library->fontset_registry = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI LibrarySupport(FontLibNative* library, std::uint32_t formats) {
    (void)formats;
    if (!library || !library->fontset_registry) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryOpenFont(FontLibNative* library, std::uint32_t mode, const void* fontAddress, std::uint32_t fontSize, std::uint32_t subFontIndex, std::uint32_t uniqueWord, FontObj** inoutFontObj) {
    (void)uniqueWord;
    if (!library || !fontAddress || !inoutFontObj) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto allocFn = library->iface ? library->iface->alloc : nullptr;
    const auto freeFn = library->iface ? library->iface->dealloc : nullptr;
    if (!allocFn || !freeFn) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool fromMemory = mode == 1;
    if (fromMemory) {
        if (fontSize == 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    } else if (mode == 5 || mode == 6 || mode == 7) {
        if (static_cast<const char*>(fontAddress)[0] == '\0') return SCE_FONT_ERROR_INVALID_PARAMETER;
    } else {
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    auto* ctx = static_cast<FtLibraryContext*>(library->fontset_registry);
    if (!ctx || !ctx->library) return SCE_FONT_ERROR_INVALID_LIBRARY;
    const bool systemSet = (library->sysfont_flags & SYSFONT_FLAG_SYSTEM_SET) != 0;
    FT_Library* ftLibrary = systemSet ? &ctx->hostLibrary : &ctx->library;
    if (systemSet && !SystemFontLibrary(ctx)) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    FT_Face face = nullptr;
    FT_Error error;
    if (fromMemory) {
        error = FT_New_Memory_Face(*ftLibrary, static_cast<const FT_Byte*>(fontAddress), static_cast<FT_Long>(fontSize), static_cast<FT_Long>(subFontIndex), &face);
    } else {
        error = FT_New_Face(*ftLibrary, static_cast<const char*>(fontAddress), static_cast<FT_Long>(subFontIndex), &face);
    }
    if (error != 0 || !face) {
        if (fromMemory || error == FT_Err_Unknown_File_Format) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        return SCE_FONT_ERROR_FS_OPEN_FAILED;
    }
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    auto* obj = static_cast<FontObj*>(allocFn(library->alloc_ctx, sizeof(FontObj)));
    if (!obj) {
        FT_Done_Face(face);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    std::memset(obj, 0, sizeof(FontObj));
    obj->refcount = 1;
    obj->sub_font_index = subFontIndex;
    obj->prev = nullptr;
    obj->next = *inoutFontObj;
    if (obj->next) obj->next->prev = obj;
    obj->ft_face = face;
    obj->ft_ctx_0x58 = ftLibrary;
    *inoutFontObj = obj;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryCloseFont(FontObj* obj, std::uint32_t flags) {
    (void)flags;
    if (!obj) return SCE_FONT_ERROR_FATAL;
    if (obj->refcount > 1) {
        --obj->refcount;
        return SCE_FONT_OK;
    }
    FT_Face face = FaceOf(obj);
    FtLibraryContext* ctx = face && face->memory ? static_cast<FtLibraryContext*>(face->memory->user) : nullptr;
    const auto freeFn = ctx && ctx->iface ? ctx->iface->dealloc : nullptr;
    if (face) {
        FT_Done_Face(face);
        obj->ft_face = nullptr;
    }
    if (!freeFn) return SCE_FONT_ERROR_FATAL;
    FontObj* next = obj->next;
    FontObj* prev = obj->prev;
    if (prev) prev->next = next;
    if (next) next->prev = prev;
    freeFn(ctx->allocCtx, obj);
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetFaceScale(FontObj* obj, std::uint16_t* outUnitsPerEm, float* outScale) {
    if (!obj || !outUnitsPerEm || !outScale) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const std::uint16_t units = face->units_per_EM;
    *outUnitsPerEm = units;
    *outScale = static_cast<float>(units) * ONE_OVER_64;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetFaceMetric(FontObj* obj, std::uint32_t metricId, std::uint16_t* outMetric) {
    if (!obj || !outMetric) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const std::uint16_t units = face->units_per_EM;
    if (metricId == 0x0E00) {
        *outMetric = units;
        return SCE_FONT_OK;
    }
    if (metricId == 0xEA00) {
        if (const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2))) {
            *outMetric = static_cast<std::uint16_t>(os2->sTypoAscender);
            return SCE_FONT_OK;
        }
        *outMetric = units;
        return SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
    }
    *outMetric = 0;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetGlyphsCount(FontObj* obj, std::uint32_t* outCount) {
    if (!obj || !outCount) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face || face->num_glyphs < 0) return SCE_FONT_ERROR_FATAL;
    *outCount = static_cast<std::uint32_t>(face->num_glyphs);
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetGlyphIndex(FontObj* obj, std::uint32_t codepoint, std::uint32_t* outGlyphIndex) {
    if (!obj || !outGlyphIndex) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face) {
        *outGlyphIndex = 0;
        return SCE_FONT_ERROR_FATAL;
    }
    const std::uint32_t glyphIndex = ResolveGlyphIndexWithFallback(face, codepoint);
    *outGlyphIndex = glyphIndex;
    return glyphIndex ? SCE_FONT_OK : SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
}

int APS5_VABI LibrarySetCharSizeWithDpi(FontObj* obj, std::uint32_t dpiX, std::uint32_t dpiY, float scaleX, float scaleY, float* outScaleX, float* outScaleY) {
    if (!obj || !outScaleX || !outScaleY) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const auto charW = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleX * 64.0f));
    const auto charH = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleY * 64.0f));
    if (SetCharSizeCompat(face, charW, charH, dpiX, dpiY) != 0) return SCE_FONT_ERROR_FATAL;
    if (!face->size) return SCE_FONT_ERROR_FATAL;
    const std::uint16_t units = face->units_per_EM;
    *outScaleX = FixedMulUnitsToF26Dot6(static_cast<std::int64_t>(face->size->metrics.x_scale), units);
    *outScaleY = FixedMulUnitsToF26Dot6(static_cast<std::int64_t>(face->size->metrics.y_scale), units);
    return SCE_FONT_OK;
}

int APS5_VABI LibrarySetCharSizeDefaultDpi(FontObj* obj, float scaleX, float scaleY, float* outScaleX, float* outScaleY) {
    if (!obj || !outScaleX || !outScaleY) return SCE_FONT_ERROR_FATAL;
    return LibrarySetCharSizeWithDpi(obj, 0x48, 0x48, scaleX, scaleY, outScaleX, outScaleY);
}

int APS5_VABI LibraryComputeLayout(FontObj* obj, const StyleStateBlock* style, std::uint8_t* outWords) {
    if (!obj || !style || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face || !face->size) return SCE_FONT_ERROR_FATAL;
    const float effectWidth = style->effect_weight_x;
    const float effectHeight = style->effect_weight_y;
    const float slant = style->slant_ratio;
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const std::int64_t yShift = obj->shift_cache_y;
    const std::int64_t xShift = obj->shift_cache_x;
    const auto unitsPerEm = static_cast<std::int64_t>(static_cast<std::uint16_t>(face->units_per_EM));
    std::int32_t yMinPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.yMin) + yShift, yScale);
    std::int32_t yMaxPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.yMax) + yShift, yScale);
    std::int32_t halfEffectWidthPx = 0;
    std::int32_t leftAdjustPx = 0;
    if (effectWidth != 0.0f) {
        const std::int32_t unitsScaledX = TruncFixed(unitsPerEm * xScale);
        halfEffectWidthPx = TruncateFloatToInt(effectWidth * static_cast<float>(unitsScaledX)) / 2;
        leftAdjustPx = -halfEffectWidthPx;
    }
    float outEffectHeight = 0.0f;
    if (effectHeight != 0.0f) {
        const std::int32_t unitsScaledY = TruncFixed(unitsPerEm * yScale);
        const std::int32_t halfEffectHeightPx = TruncateFloatToInt(effectHeight * static_cast<float>(unitsScaledY)) / 2;
        outEffectHeight = static_cast<float>(halfEffectHeightPx) * ONE_OVER_64;
        yMinPx -= halfEffectHeightPx;
        yMaxPx += halfEffectHeightPx;
    }
    if (slant != 0.0f) {
        const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slant * 65536.0f));
        leftAdjustPx += RoundMulFixed(yMinPx, shear);
        halfEffectWidthPx += RoundMulFixed(yMaxPx, shear);
    }
    StoreFloat(outWords, HORIZONTAL_EFFECT_HEIGHT, outEffectHeight);
    StoreFloat(outWords, HORIZONTAL_LEFT_ADJUST, static_cast<float>(leftAdjustPx) * ONE_OVER_64);
    StoreFloat(outWords, HORIZONTAL_HALF_EFFECT_WIDTH, static_cast<float>(halfEffectWidthPx) * ONE_OVER_64);
    const std::int32_t xMinPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMin) + xShift, xScale);
    const std::int32_t xMaxPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMax) + xShift, xScale);
    StoreFloat(outWords, HORIZONTAL_LINE_ADVANCE, static_cast<float>(yMaxPx - yMinPx) * ONE_OVER_64);
    StoreFloat(outWords, HORIZONTAL_BASELINE, static_cast<float>(yMaxPx) * ONE_OVER_64);
    StoreFloat(outWords, HORIZONTAL_X_BOUND_LO, static_cast<float>(xMinPx + leftAdjustPx) * ONE_OVER_64);
    StoreFloat(outWords, HORIZONTAL_X_BOUND_HI, static_cast<float>(xMaxPx + halfEffectWidthPx) * ONE_OVER_64);
    const std::int32_t maxAdvanceWidthPx = RoundMulFixed(static_cast<std::int64_t>(face->max_advance_width) + xShift, xScale);
    StoreFloat(outWords, HORIZONTAL_MAX_ADVANCE_WIDTH, static_cast<float>(maxAdvanceWidthPx) * ONE_OVER_64);
    float caretRiseAdjust = 0.0f;
    if (const auto* hhea = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA))) {
        const std::int64_t caretRiseUnits = xShift + static_cast<std::int64_t>(hhea->caret_Slope_Rise);
        const std::int32_t caretRisePx = TruncFixed(caretRiseUnits * xScale);
        caretRiseAdjust = static_cast<float>(caretRisePx - halfEffectWidthPx) * ONE_OVER_64;
    }
    StoreFloat(outWords, HORIZONTAL_CARET_RISE_ADJUST, caretRiseAdjust);
    return SCE_FONT_OK;
}

int APS5_VABI LibraryComputeLayoutAlt(FontObj* obj, const StyleStateBlock* style, std::uint8_t* outWords) {
    if (!obj || !style || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face || !face->size) return SCE_FONT_ERROR_FATAL;
    const float effectWidth = style->effect_weight_x;
    const float effectHeight = style->effect_weight_y;
    const float slant = style->slant_ratio;
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto* vhea = static_cast<const TT_VertHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_VHEA));
    const std::int64_t yShift = obj->shift_cache_y;
    const std::int64_t xShift = obj->shift_cache_x;
    const auto units = static_cast<std::uint16_t>(face->units_per_EM);
    std::int64_t yAscender = 0;
    std::int64_t yDescender = 0;
    if (vhea) {
        yAscender = static_cast<std::int64_t>(vhea->Ascender) + yShift;
        yDescender = static_cast<std::int64_t>(vhea->Descender) + yShift;
    }
    const std::int32_t scaledLeft = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMin) + xShift, xScale);
    const std::int32_t scaledRight = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMax) + xShift, xScale);
    std::int32_t xMinScaled = scaledLeft;
    std::int32_t xAbsMax = -xMinScaled;
    if (xAbsMax < scaledRight) {
        xAbsMax = scaledRight;
        xMinScaled = -scaledRight;
    }
    float outEffectWidth = 0.0f;
    if (effectWidth != 0.0f) {
        const auto unitsScaled = static_cast<std::int32_t>(TruncMulUnits(xScale, units));
        const std::int32_t effect = TruncateFloatToInt(effectWidth * static_cast<float>(unitsScaled)) / 2;
        outEffectWidth = static_cast<float>(effect) * ONE_OVER_64;
    }
    if (effectHeight != 0.0f) {
        const auto unitsScaled = static_cast<std::int32_t>(TruncMulUnits(yScale, units));
        const std::int32_t delta = TruncateFloatToInt(effectHeight * static_cast<float>(unitsScaled)) / 2;
        yAscender -= delta;
        yDescender += delta;
    }
    float outSlantA = 0.0f;
    float outSlantB = 0.0f;
    if (slant != 0.0f) {
        const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slant * 65536.0f));
        const std::int64_t signAdjustMin = yAscender < 1 ? 0x10000LL : 0LL;
        std::int64_t roundedMin = signAdjustMin + (-yAscender) * shear - 0x8000LL;
        if (roundedMin < 0) roundedMin = signAdjustMin + 0x7FFFLL + (-yAscender) * shear;
        const std::int64_t signAdjustMax = yDescender >= 0 ? 0x10000LL : 0LL;
        std::int64_t roundedMax = signAdjustMax + yDescender * shear - 0x8000LL;
        if (roundedMax < 0) roundedMax = signAdjustMax + 0x7FFFLL + yDescender * shear;
        const auto roundedMaxBits = static_cast<std::uint64_t>(roundedMax);
        const auto roundedMinBits = static_cast<std::uint64_t>(roundedMin);
        const std::uint64_t roundedMaxHigh = roundedMaxBits >> 16;
        const bool maxRoundBitClear = ((roundedMaxBits >> 15) & 1u) == 0;
        std::uint64_t selectedHigh = roundedMaxHigh;
        auto selectedLow = static_cast<std::int32_t>(roundedMinBits >> 16);
        if (maxRoundBitClear && roundedMaxHigh != 0) {
            selectedHigh = roundedMinBits >> 16;
            selectedLow = static_cast<std::int32_t>(roundedMaxBits >> 16);
        }
        outSlantA = static_cast<float>(selectedLow) * ONE_OVER_64;
        outSlantB = static_cast<float>(static_cast<std::int32_t>(selectedHigh)) * ONE_OVER_64;
    }
    StoreFloat(outWords, VERTICAL_COLUMN_ADVANCE, static_cast<float>(xAbsMax - xMinScaled) * ONE_OVER_64);
    StoreFloat(outWords, VERTICAL_BASELINE_OFFSET_X, static_cast<float>(-xAbsMax) * ONE_OVER_64);
    StoreFloat(outWords, VERTICAL_METRICS_0X08, static_cast<float>(RoundMulFixed(yAscender, xScale)) * ONE_OVER_64);
    StoreFloat(outWords, VERTICAL_METRICS_0X0C, static_cast<float>(RoundMulFixed(yDescender, xScale)) * ONE_OVER_64);
    const std::int32_t advanceHeight = RoundMulFixed(static_cast<std::int64_t>(face->max_advance_height) + yShift, yScale);
    StoreFloat(outWords, VERTICAL_ADVANCE_HEIGHT, static_cast<float>(advanceHeight) * ONE_OVER_64);
    StoreFloat(outWords, VERTICAL_DECORATION_SPAN, outEffectWidth);
    StoreFloat(outWords, VERTICAL_DECORATION_0X08, outSlantB);
    StoreFloat(outWords, VERTICAL_DECORATION_0X0C, outSlantA);
    return SCE_FONT_OK;
}

void StoreSeedVector(FontObj* obj) {
    const auto seedLow = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair & 0xFFFFFFFFu));
    const auto seedHigh = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair >> 32));
    obj->layout_seed_vec[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedLow));
    obj->layout_seed_vec[1] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedHigh));
}

void WriteOutlineSizes(FT_GlyphSlot slot, std::uint64_t* outWords) {
    const auto contours = static_cast<std::uint32_t>(slot->outline.n_contours);
    const auto points = static_cast<std::uint32_t>(slot->outline.n_points);
    const std::uint32_t size4 = points * 0x10u + 0x68u + ((points + 0x0Fu + contours * 2u) & 0xFFFFFFF0u);
    const std::uint32_t size8 = ((points + 0x2Bu + contours * 2u) & 0xFFFFFFFCu) + 0x10u + points * 4u;
    auto* out = reinterpret_cast<std::uint8_t*>(outWords);
    WriteU16(out, 0, static_cast<std::uint16_t>(contours));
    WriteU16(out, 2, static_cast<std::uint16_t>(points));
    WriteU32(out, 4, size4);
    WriteU32(out, 8, size8);
}

int APS5_VABI LibraryLoadGlyphCached(FontObj* obj, std::uint32_t glyphIndex, std::int32_t mode, std::uint64_t* outWords) {
    if (outWords) {
        outWords[0] = 0;
        outWords[1] = 0;
    }
    if (!obj || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face || !face->glyph) return SCE_FONT_ERROR_FATAL;
    const auto unitsPerEm = static_cast<std::uint16_t>(face->units_per_EM);
    const bool cachedMatch = obj->cached_glyph_index_0x64 == static_cast<std::int32_t>(glyphIndex) && obj->cached_units_x_0x68 == unitsPerEm && obj->cached_units_y_0x70 == obj->cached_units_x_0x68 && mode == 0;
    if (cachedMatch) {
        obj->shift_cache_x = obj->shift_units_x;
        obj->shift_cache_y = obj->shift_units_y;
        StoreSeedVector(obj);
        WriteOutlineSizes(face->glyph, outWords);
        return SCE_FONT_OK;
    }
    const FT_Int32 loadFlags = (mode == 0 ? FT_LOAD_NO_SCALE : 0) | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT;
    const FT_Error error = FT_Load_Glyph(face, glyphIndex, loadFlags);
    if (error != 0) {
        obj->cached_glyph_index_0x64 = 0;
        obj->cached_units_x_0x68 = 0;
        obj->cached_units_y_0x70 = 0;
        obj->layout_seed_vec = {};
        obj->layout_scale_vec = {};
        return error == FT_Err_Out_Of_Memory ? SCE_FONT_ERROR_ALLOCATION_FAILED : SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    }
    if (!face->glyph) return SCE_FONT_ERROR_FATAL;
    obj->cached_glyph_index_0x64 = static_cast<std::int32_t>(glyphIndex);
    if (mode == 0) {
        obj->cached_units_x_0x68 = unitsPerEm;
        obj->cached_units_y_0x70 = unitsPerEm;
        obj->shift_cache_x = obj->shift_units_x;
        obj->shift_cache_y = obj->shift_units_y;
        StoreSeedVector(obj);
        obj->layout_scale_vec[0] = 0x10000;
        obj->layout_scale_vec[1] = 0x10000;
    } else {
        if (!face->size) return SCE_FONT_ERROR_FATAL;
        const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
        const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
        obj->cached_units_x_0x68 = static_cast<std::uint64_t>(TruncMulUnits(xScale, unitsPerEm));
        obj->cached_units_y_0x70 = static_cast<std::uint64_t>(TruncMulUnits(yScale, unitsPerEm));
        obj->shift_cache_x = RoundMulFixed(obj->shift_units_x, xScale);
        obj->shift_cache_y = RoundMulFixed(obj->shift_units_y, yScale);
        const auto seedLow = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair & 0xFFFFFFFFu));
        const auto seedHigh = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair >> 32));
        obj->layout_seed_vec[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(RoundMulFixed(seedLow, xScale)));
        obj->layout_seed_vec[1] = static_cast<std::uint64_t>(static_cast<std::int64_t>(RoundMulFixed(seedHigh, yScale)));
        obj->layout_scale_vec[0] = static_cast<std::uint64_t>(xScale);
        obj->layout_scale_vec[1] = static_cast<std::uint64_t>(yScale);
    }
    WriteOutlineSizes(face->glyph, outWords);
    return SCE_FONT_OK;
}

std::int64_t RoundMulFixed64(std::int64_t value, std::int64_t fixed16) {
    return static_cast<std::int64_t>(RoundMulFixed(value, fixed16));
}

int APS5_VABI LibraryGetGlyphMetrics(FontObj* obj, std::uint32_t* optParam2, std::uint8_t mode, std::uint8_t* outParams, FontGlyphMetrics* outMetrics) {
    if (!obj || !outParams || !outMetrics) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(obj);
    if (!face || !face->glyph || !face->size) return SCE_FONT_ERROR_FATAL;
    const FT_GlyphSlot slot = face->glyph;
    std::memset(outParams, 0, 0x48);
    const void* outlineBlob = &slot->outline;
    outParams[0] = 0xF2;
    WritePointer(outParams, 0x08, slot);
    if (optParam2) {
        outlineBlob = optParam2;
        outParams[0] = 0xF0;
        WritePointer(outParams, 0x08, optParam2);
    }
    const auto faceFlagBit = static_cast<std::uint8_t>((static_cast<std::uint32_t>(face->face_flags) >> 5) & 1u);
    const bool hasSeedVector = obj->layout_seed_vec[0] != 0 || obj->layout_seed_vec[1] != 0;
    outParams[2] = static_cast<std::uint8_t>(0xF0u | (hasSeedVector ? 0x08u : 0u) | faceFlagBit);
    outParams[3] = 0;
    WriteU16(outParams, 0x04, static_cast<std::uint16_t>(face->units_per_EM));
    WritePointer(outParams, 0x10, outlineBlob);
    WritePointer(outParams, 0x18, outMetrics);
    float styleShiftX = 0.0f;
    float styleShiftY = 0.0f;
    if (obj->reserved_0x04 != 0) {
        styleShiftX = static_cast<float>(slot->metrics.vertBearingX - slot->metrics.horiBearingX);
        styleShiftY = static_cast<float>(-(slot->metrics.horiBearingY + slot->metrics.vertBearingY));
        outParams[1] = 1;
    } else {
        outParams[1] = 0;
    }
    StoreFloat(outParams, 0x20, styleShiftX);
    StoreFloat(outParams, 0x24, styleShiftY);
    StoreFloat(outParams, 0x28, 1.0f);
    const auto shiftX = static_cast<std::int32_t>(obj->shift_cache_x);
    const auto shiftY = static_cast<std::int32_t>(obj->shift_cache_y);
    const auto seedX = static_cast<std::int32_t>(obj->layout_seed_vec[0]);
    const auto seedY = static_cast<std::int32_t>(obj->layout_seed_vec[1]);
    WriteS32(outParams, 0x2C, shiftX);
    WriteS32(outParams, 0x30, shiftY);
    WriteS32(outParams, 0x34, seedX);
    WriteS32(outParams, 0x38, seedY);
    StoreFloat(outParams, 0x3C, static_cast<float>(static_cast<std::int64_t>(obj->cached_units_x_0x68)) * ONE_OVER_64);
    StoreFloat(outParams, 0x40, static_cast<float>(static_cast<std::int64_t>(obj->cached_units_y_0x70)) * ONE_OVER_64);
    WriteU32(outParams, 0x44, 0);
    auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto objScaleX = static_cast<std::int64_t>(obj->layout_scale_vec[0]);
    const auto objScaleY = static_cast<std::int64_t>(obj->layout_scale_vec[1]);
    if (objScaleX != 0 && objScaleX != 0x10000) xScale = (xScale << 16) / objScaleX;
    if (objScaleY != 0 && objScaleY != 0x10000) yScale = (yScale << 16) / objScaleY;
    const bool applyScale = (mode & 0x0Fu) != 0;
    const auto scaleIf = [&](std::int64_t value, std::int64_t scale) {
        return applyScale ? RoundMulFixed64(value, scale) : value;
    };
    const auto widthRaw = static_cast<std::int64_t>(slot->metrics.width);
    const auto heightRaw = static_cast<std::int64_t>(slot->metrics.height);
    const auto horiBearingXRaw = static_cast<std::int64_t>(slot->metrics.horiBearingX);
    const auto horiAdvanceRaw = static_cast<std::int64_t>(slot->metrics.horiAdvance);
    const auto vertBearingXRaw = static_cast<std::int64_t>(slot->metrics.vertBearingX);
    const auto vertBearingYRaw = static_cast<std::int64_t>(slot->metrics.vertBearingY);
    const auto vertAdvanceRaw = static_cast<std::int64_t>(slot->metrics.vertAdvance);
    const std::int64_t horiAdvanceUnscaled = seedX + horiAdvanceRaw;
    const std::int64_t vertAdvanceUnscaled = seedY + vertAdvanceRaw;
    slot->metrics.horiAdvance = static_cast<FT_Pos>(horiAdvanceUnscaled);
    slot->metrics.vertAdvance = static_cast<FT_Pos>(vertAdvanceUnscaled);
    if (widthRaw == 0 && heightRaw == 0) {
        *outMetrics = {};
        outMetrics->Horizontal.advance = FromF26Dot6(scaleIf(horiAdvanceUnscaled, xScale));
        outMetrics->Vertical.advance = FromF26Dot6(scaleIf(vertAdvanceUnscaled, yScale));
        return SCE_FONT_OK;
    }
    const std::int64_t topShift = heightRaw == 0 ? 0 : shiftY;
    std::int64_t left = horiBearingXRaw + shiftX;
    std::int64_t top = vertBearingYRaw + topShift;
    std::int64_t right = left + widthRaw;
    std::int64_t bottom = top - heightRaw;
    left = scaleIf(left, xScale);
    top = scaleIf(top, yScale);
    right = scaleIf(right, xScale);
    bottom = scaleIf(bottom, yScale);
    const std::int64_t horiAdvance = scaleIf(horiAdvanceUnscaled, xScale);
    const std::int64_t vertAdvance = scaleIf(vertAdvanceUnscaled, yScale);
    const std::int64_t outWidth = right - left;
    const std::int64_t outHeight = top - bottom;
    std::int64_t vertBearingX;
    std::int64_t vertBearingY;
    if ((static_cast<std::uint32_t>(face->face_flags) & 0x20u) == 0) {
        vertBearingX = left - horiAdvance / 2;
        vertBearingY = (vertAdvance - outHeight) / 2;
    } else {
        if (seedX == 0) {
            vertBearingX = scaleIf(shiftX + vertBearingXRaw, xScale);
        } else {
            slot->metrics.vertBearingX = static_cast<FT_Pos>(horiBearingXRaw - horiAdvanceUnscaled / 2);
            vertBearingX = left - horiAdvance / 2;
            StoreFloat(outParams, 0x20, static_cast<float>(-(horiAdvanceUnscaled / 2)));
        }
        vertBearingY = scaleIf(topShift + vertBearingYRaw, yScale);
    }
    outMetrics->width = FromF26Dot6(outWidth);
    outMetrics->height = FromF26Dot6(outHeight);
    outMetrics->Horizontal.bearingX = FromF26Dot6(left);
    outMetrics->Horizontal.bearingY = FromF26Dot6(top);
    outMetrics->Horizontal.advance = FromF26Dot6(horiAdvance);
    outMetrics->Vertical.bearingX = FromF26Dot6(vertBearingX);
    outMetrics->Vertical.bearingY = FromF26Dot6(vertBearingY);
    outMetrics->Vertical.advance = FromF26Dot6(vertAdvance);
    if ((mode & 0x06u) != 0 && obj->font_handle) {
        const auto* handle = reinterpret_cast<const FontHandleNative*>(obj->font_handle);
        const StyleStateBlock* style = (mode & 0x04u) ? &handle->cached_style.state : &handle->style;
        const float slantRatio = style->slant_ratio;
        if ((slantRatio != 0.0f || std::isnan(slantRatio)) && slot->outline.points && slot->outline.n_points > 0) {
            const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slantRatio * 65536.0f));
            std::int64_t minX = std::numeric_limits<std::int64_t>::max();
            std::int64_t maxX = std::numeric_limits<std::int64_t>::min();
            for (int i = 0; i < slot->outline.n_points; ++i) {
                const FT_Vector point = slot->outline.points[i];
                const std::int64_t px = scaleIf(static_cast<std::int64_t>(point.x) + shiftX, xScale);
                const std::int64_t py = scaleIf(static_cast<std::int64_t>(point.y) + shiftY, yScale);
                const std::int64_t slantedX = px + RoundMulFixed64(py, shear);
                minX = std::min(minX, slantedX);
                maxX = std::max(maxX, slantedX);
            }
            if (minX <= maxX) {
                outMetrics->width = FromF26Dot6(maxX - minX);
                outMetrics->Horizontal.bearingX = FromF26Dot6(minX);
            }
        }
        const float effectX = style->effect_weight_x;
        const float effectY = style->effect_weight_y;
        if (effectX != 0.0f || std::isnan(effectX) || effectY != 0.0f || std::isnan(effectY)) {
            const float dx = effectX * obj->scale_x_0x50 * 0.5f;
            const float dy = effectY * obj->scale_y_0x54 * 0.5f;
            outMetrics->width = outMetrics->width + dx + dx;
            outMetrics->height = outMetrics->height + dy + dy;
            outMetrics->Horizontal.bearingX -= dx;
            outMetrics->Horizontal.bearingY += dy;
            outMetrics->Vertical.bearingX -= dx;
            outMetrics->Vertical.bearingY -= dy;
            if (outMetrics->Horizontal.advance != 0.0f || std::isnan(outMetrics->Horizontal.advance)) outMetrics->Horizontal.advance += dx;
            if (outMetrics->Vertical.advance != 0.0f || std::isnan(outMetrics->Vertical.advance)) outMetrics->Vertical.advance += dy;
        }
    }
    return SCE_FONT_OK;
}

int APS5_VABI LibraryApplyGlyphAdjust(FontObj* obj, std::uint32_t p2, std::uint32_t glyphIndex, std::int32_t p4, std::int32_t p5, std::uint32_t* inoutGlyphIndex) {
    (void)p2;
    if (!obj || !inoutGlyphIndex) return SCE_FONT_ERROR_FATAL;
    *inoutGlyphIndex = glyphIndex;
    obj->shift_units_x = p4;
    obj->shift_units_y = p5;
    obj->layout_seed_pair = 0;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryConfigureGlyph(FontObj* obj, std::uint32_t* inParams, std::int32_t mode, std::uint32_t* inoutState) {
    if (!obj || !inParams || !inoutState) return SCE_FONT_ERROR_FATAL;
    if (*inParams != 0) reinterpret_cast<std::uint8_t*>(inoutState)[3] |= 0x80;
    obj->glyph_cfg_word_0x130 = *inParams;
    obj->glyph_cfg_mode_0x134 = static_cast<std::uint8_t>(mode);
    obj->glyph_cfg_byte_0x136 = 0;
    obj->glyph_cfg_byte_0x135 = 0;
    return SCE_FONT_OK;
}

int APS5_VABI RendererCreate(RendererNative* renderer) {
    if (!renderer) return SCE_FONT_ERROR_INVALID_RENDERER;
    auto* ft = reinterpret_cast<RendererFt*>(renderer);
    ft->ft_backend.renderer_header_0x10 = &ft->base.mem_kind;
    ft->ft_backend.unknown_0x08 = 0;
    ft->ft_backend.unknown_0x10 = 0;
    ft->ft_backend.unknown_0x18 = 0;
    ft->ft_backend.initialized_marker = ft;
    return SCE_FONT_OK;
}

int APS5_VABI RendererDestroy(RendererNative* renderer) {
    if (!renderer) return SCE_FONT_ERROR_INVALID_RENDERER;
    reinterpret_cast<RendererFt*>(renderer)->ft_backend.initialized_marker = nullptr;
    return SCE_FONT_OK;
}

std::uint64_t APS5_VABI RendererQuery(RendererNative* renderer, std::uint8_t* params, std::int64_t* outPtr, std::uint8_t* outVector) {
    (void)renderer;
    (void)params;
    if (outVector) std::memset(outVector, 0, 16);
    if (outPtr) *outPtr = 0;
    return 0;
}

SysDriver MakeDriverTable() {
    SysDriver driver{};
    driver.pixel_resolution = &LibraryGetPixelResolution;
    driver.init = &LibraryInit;
    driver.term = &LibraryTerm;
    driver.support_formats = &LibrarySupport;
    driver.open = &LibraryOpenFont;
    driver.close = &LibraryCloseFont;
    driver.scale = &LibraryGetFaceScale;
    driver.metric = &LibraryGetFaceMetric;
    driver.glyphs_count = &LibraryGetGlyphsCount;
    driver.glyph_index = &LibraryGetGlyphIndex;
    driver.set_char_with_dpi = &LibrarySetCharSizeWithDpi;
    driver.set_char_default_dpi = &LibrarySetCharSizeDefaultDpi;
    driver.compute_layout = &LibraryComputeLayout;
    driver.compute_layout_alt = &LibraryComputeLayoutAlt;
    driver.load_glyph_cached = &LibraryLoadGlyphCached;
    driver.get_glyph_metrics = &LibraryGetGlyphMetrics;
    driver.apply_glyph_adjust = &LibraryApplyGlyphAdjust;
    driver.configure_glyph = &LibraryConfigureGlyph;
    return driver;
}

const SysDriver driverTable = MakeDriverTable();
const RendererSelection rendererTable{0, sizeof(RendererFt), &RendererCreate, &RendererDestroy, &RendererQuery};

}

const Font::SysDriver* FontFt::DriverTable() {
    return &driverTable;
}

const Font::RendererSelection* FontFt::RendererTable() {
    return &rendererTable;
}
