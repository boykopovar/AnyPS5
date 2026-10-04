// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "prx/libSceFont/include/FontFreeType.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

using namespace Font;

struct FloatPair {
    float lo = 0.0f;
    float hi = 0.0f;
};

bool OrderedLess(float a, float b) {
    return !std::isnan(a) && !std::isnan(b) && a < b;
}

float MaxSs(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return b;
    if (a > b) return a;
    if (a < b) return b;
    if (a == 0.0f && b == 0.0f && std::signbit(a) != std::signbit(b)) return std::signbit(a) ? b : a;
    return a;
}

float FlipSign(float value) {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) ^ 0x80000000u);
}

void UpdateShiftCache(FontObj* obj) {
    obj->shift_cache_x = obj->shift_units_x;
    obj->shift_cache_y = obj->shift_units_y;
    const auto seedLow = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair & 0xFFFFFFFFu));
    const auto seedHigh = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair >> 32));
    obj->layout_seed_vec[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedLow));
    obj->layout_seed_vec[1] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedHigh));
    obj->layout_scale_vec[0] = 0x10000;
    obj->layout_scale_vec[1] = 0x10000;
}

int SetDriverCharSize(const SysDriver* driver, FontObj* obj, const StyleStateBlock* style, float& scaleX, float& scaleY) {
    if (style->scale_unit == 0) return driver->set_char_default_dpi(obj, scaleX, scaleY, &scaleX, &scaleY);
    return driver->set_char_with_dpi(obj, style->dpi_x, style->dpi_y, scaleX, scaleY, &scaleX, &scaleY);
}

void ClearHorizontalLayout(std::uint8_t* outWords) {
    if (!outWords) return;
    std::memset(outWords, 0, 0x10);
    std::memset(outWords + 0x18, 0, 0x0C);
}

void ClearVerticalLayout(std::uint8_t* outWords) {
    if (!outWords) return;
    std::memset(outWords, 0, 0x10);
    std::memset(outWords + 0x14, 0, 0x0C);
}

const StyleStateBlock* ResolveSurfaceStyleState(const StyleStateBlock* cachedState, const FontRenderSurface* surface, StyleStateBlock& temporary) {
    const auto* systemUse = reinterpret_cast<const RenderSurfaceSystemUse*>(surface->reserved_q);
    if ((surface->styleFlag & 0x1) == 0 || !ValidStyleFrame(systemUse->styleframe)) return cachedState;
    const FontStyleFrame* frame = systemUse->styleframe;
    temporary = *cachedState;
    if ((frame->flags1 & STYLE_FRAME_FLAG_SCALE) != 0) {
        temporary.scale_unit = frame->scaleUnit;
        temporary.scale_w = frame->scalePixelW;
        temporary.scale_h = frame->scalePixelH;
        temporary.dpi_x = frame->hDpi ? frame->hDpi : 0x48;
        temporary.dpi_y = frame->vDpi ? frame->vDpi : 0x48;
    } else {
        const std::uint32_t wantX = frame->hDpi == 0 ? temporary.dpi_x : frame->hDpi;
        const std::uint32_t wantY = frame->vDpi == 0 ? temporary.dpi_y : frame->vDpi;
        temporary.dpi_x = wantX != 0 ? 0x48 : 0;
        temporary.dpi_y = wantY != 0 ? 0x48 : 0;
    }
    if ((frame->flags1 & STYLE_FRAME_FLAG_SLANT) != 0) temporary.slant_ratio = frame->slantRatio;
    if ((frame->flags1 & STYLE_FRAME_FLAG_WEIGHT) != 0) {
        temporary.effect_weight_x = frame->effectWeightX;
        temporary.effect_weight_y = frame->effectWeightY;
    }
    return &temporary;
}

void FillImageMetrics(FontRenderOutput* result, const FontGlyphMetrics* metrics, float x, float y) {
    const float leftF = x + metrics->Horizontal.bearingX;
    const float topF = y + metrics->Horizontal.bearingY;
    const float rightF = leftF + metrics->width;
    const float bottomF = topF - metrics->height;
    const int left = FloorToInt(leftF);
    const int top = CeilToInt(topF);
    const int right = CeilToInt(rightF);
    const int bottom = FloorToInt(bottomF);
    const float advanceF = x + metrics->Horizontal.advance;
    const int advance = CeilToInt(advanceF);
    result->ImageMetrics.bearingX = static_cast<float>(left) - x;
    result->ImageMetrics.bearingY = static_cast<float>(top) - y;
    result->ImageMetrics.advance = static_cast<float>(advance) - x;
    int stride = advance;
    if (advanceF != static_cast<float>(advance)) {
        const int candidate = static_cast<int>((static_cast<float>(right) - rightF) + advanceF);
        stride = right + 1;
        if (advanceF <= rightF) stride = candidate;
        if (stride < candidate) stride = candidate;
    }
    result->ImageMetrics.stride = static_cast<float>(stride) - x;
    result->ImageMetrics.width = static_cast<std::uint32_t>(std::max(0, right - left));
    result->ImageMetrics.height = static_cast<std::uint32_t>(std::max(0, top - bottom));
}

int RenderGlyphIndexToSurface(FontObj& obj, std::uint32_t glyphIndex, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    ClearRenderOutputs(metrics, result);
    if (!surface->buffer || surface->width <= 0 || surface->height <= 0 || surface->widthByte <= 0 || surface->pixelSizeByte <= 0) return SCE_FONT_ERROR_NO_SUPPORT_SURFACE;
    const int bytesPerPixel = surface->pixelSizeByte;
    if (bytesPerPixel != 1 && bytesPerPixel != 4) return SCE_FONT_ERROR_NO_SUPPORT_SURFACE;
    const auto face = static_cast<FT_Face>(obj.ft_face);
    if (!face || !face->size) return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    FT_Vector delta{};
    delta.x = static_cast<FT_Pos>(static_cast<std::int32_t>((x - std::floor(x)) * 64.0f));
    delta.y = static_cast<FT_Pos>(-static_cast<std::int32_t>((y - std::floor(y)) * 64.0f));
    if (obj.shift_units_x != 0 || obj.shift_units_y != 0) {
        delta.x += static_cast<FT_Pos>(RoundMulFixed(obj.shift_units_x, static_cast<std::int64_t>(face->size->metrics.x_scale)));
        delta.y += static_cast<FT_Pos>(RoundMulFixed(obj.shift_units_y, static_cast<std::int64_t>(face->size->metrics.y_scale)));
    }
    FT_Set_Transform(face, nullptr, &delta);
    const FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT | FT_LOAD_RENDER);
    FT_Set_Transform(face, nullptr, nullptr);
    if (error != 0 || !face->glyph) return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    const FT_GlyphSlot slot = face->glyph;
    const int glyphW = static_cast<int>(slot->bitmap.width);
    const int glyphH = static_cast<int>(slot->bitmap.rows);
    metrics->width = static_cast<float>(slot->metrics.width) / 64.0f;
    metrics->height = static_cast<float>(slot->metrics.height) / 64.0f;
    metrics->Horizontal.bearingX = static_cast<float>(slot->metrics.horiBearingX) / 64.0f;
    metrics->Horizontal.bearingY = static_cast<float>(slot->metrics.horiBearingY) / 64.0f;
    metrics->Horizontal.advance = static_cast<float>(slot->metrics.horiAdvance) / 64.0f;
    std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(glyphW) * static_cast<std::size_t>(glyphH));
    if (glyphW > 0 && glyphH > 0) {
        const int pitch = slot->bitmap.pitch;
        const auto* source = slot->bitmap.buffer;
        for (int row = 0; row < glyphH; ++row) {
            const std::uint8_t* sourceRow = source + static_cast<std::ptrdiff_t>(row) * pitch;
            std::uint8_t* targetRow = bitmap.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(glyphW);
            if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                for (int column = 0; column < glyphW; ++column) targetRow[column] = (sourceRow[column >> 3] & (0x80u >> (column & 7))) ? 0xFF : 0x00;
            } else {
                std::memcpy(targetRow, sourceRow, static_cast<std::size_t>(glyphW));
            }
        }
    }
    const int destX = static_cast<int>(std::floor(x)) + slot->bitmap_left;
    const int destY = static_cast<int>(std::floor(y)) - slot->bitmap_top;
    const int clipX0 = std::clamp(static_cast<int>(surface->sc_x0), 0, surface->width);
    const int clipY0 = std::clamp(static_cast<int>(surface->sc_y0), 0, surface->height);
    const int clipX1 = std::clamp(static_cast<int>(surface->sc_x1), 0, surface->width);
    const int clipY1 = std::clamp(static_cast<int>(surface->sc_y1), 0, surface->height);
    int updateX = destX;
    int updateY = destY;
    int updateW = 0;
    int updateH = 0;
    if (glyphW > 0 && glyphH > 0 && clipX1 > clipX0 && clipY1 > clipY0) {
        auto* targetBase = static_cast<std::uint8_t*>(surface->buffer);
        const int startRow = std::max(destY, clipY0);
        const int endRow = std::min(destY + glyphH, clipY1);
        const int startColumn = std::max(destX, clipX0);
        const int endColumn = std::min(destX + glyphW, clipX1);
        updateX = startColumn;
        updateY = startRow;
        updateW = std::max(0, endColumn - startColumn);
        updateH = std::max(0, endRow - startRow);
        for (int row = startRow; row < endRow; ++row) {
            const std::uint8_t* sourceRow = bitmap.data() + static_cast<std::size_t>(row - destY) * static_cast<std::size_t>(glyphW);
            std::uint8_t* targetRow = targetBase + static_cast<std::size_t>(row) * static_cast<std::size_t>(surface->widthByte);
            for (int column = startColumn; column < endColumn; ++column) {
                const std::uint8_t coverage = sourceRow[column - destX];
                std::memset(targetRow + static_cast<std::size_t>(column) * static_cast<std::size_t>(bytesPerPixel), coverage, static_cast<std::size_t>(bytesPerPixel));
            }
        }
    }
    result->stage = nullptr;
    result->SurfaceImage.widthByte = static_cast<std::uint32_t>(surface->widthByte);
    result->SurfaceImage.pixelSizeByte = static_cast<std::uint8_t>(surface->pixelSizeByte);
    result->SurfaceImage.pixelFormat = 0;
    result->SurfaceImage.pad16 = 0;
    result->UpdateRect.x = static_cast<std::uint32_t>(std::max(updateX, 0));
    result->UpdateRect.y = static_cast<std::uint32_t>(std::max(updateY, 0));
    result->UpdateRect.w = static_cast<std::uint32_t>(std::max(updateW, 0));
    result->UpdateRect.h = static_cast<std::uint32_t>(std::max(updateH, 0));
    result->SurfaceImage.address = static_cast<std::uint8_t*>(surface->buffer) + static_cast<std::size_t>(result->UpdateRect.y) * static_cast<std::size_t>(surface->widthByte) + static_cast<std::size_t>(result->UpdateRect.x) * static_cast<std::size_t>(bytesPerPixel);
    FillImageMetrics(result, metrics, x, y);
    return SCE_FONT_OK;
}

}

int Font::ComputeHorizontalLayout(FontHandle handle, const StyleStateBlock* style, std::uint8_t* outWords) {
    ClearHorizontalLayout(outWords);
    const auto fail = [&] {
        ClearHorizontalLayout(outWords);
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    };
    auto* font = GetNativeFont(handle);
    if (!font) return fail();
    auto* library = static_cast<FontLibNative*>(font->library);
    if (!library || !FontContext(library, font)) return fail();
    const SysDriver* driver = library->sys_driver;
    if (!driver || !driver->set_char_with_dpi || !driver->set_char_default_dpi || !driver->compute_layout || !style) return fail();
    const std::uint32_t modeLow = font->flags & 0x0Fu;
    const std::uint32_t fontId = font->open_info.ctx_entry_index;
    float baselineMax = 0.0f;
    float deltaMax = 0.0f;
    float effectForBaseline = 0.0f;
    float effectForDelta = 0.0f;
    FloatPair accumulatedAdjust{};
    FloatPair accumulatedBounds{};
    if (static_cast<std::int32_t>(fontId) >= 0) {
        FontObj* head = nullptr;
        std::uint32_t lockWord = 0;
        auto* entry = AcquireFontCtxEntry(FontContext(library, font), fontId, modeLow, &head, &lockWord);
        if (!entry) return fail();
        FontObj* match = FindSubFont(head, font->open_info.sub_font_index);
        int rc = SCE_FONT_ERROR_FATAL;
        std::uint8_t layout[HORIZONTAL_LAYOUT_SIZE] = {};
        if ((lockWord & OPEN_BIT) != 0 && (lockWord & COUNT_MASK) != 0 && match) {
            float scaleX = style->scale_w;
            float scaleY = style->scale_h;
            match->font_handle = handle;
            match->shift_units_x = 0;
            match->shift_units_y = 0;
            rc = SetDriverCharSize(driver, match, style, scaleX, scaleY);
            if (rc == SCE_FONT_OK) {
                UpdateShiftCache(match);
                rc = driver->compute_layout(match, style, layout);
            }
        }
        ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
        if (rc != SCE_FONT_OK) return fail();
        const float lineAdvance = LoadFloat(layout, HORIZONTAL_LINE_ADVANCE);
        const float baseline = LoadFloat(layout, HORIZONTAL_BASELINE);
        const float effectHeight = LoadFloat(layout, HORIZONTAL_EFFECT_HEIGHT);
        const FloatPair bounds{LoadFloat(layout, HORIZONTAL_X_BOUND_LO), LoadFloat(layout, HORIZONTAL_X_BOUND_HI)};
        const FloatPair adjust{LoadFloat(layout, HORIZONTAL_HALF_EFFECT_WIDTH), LoadFloat(layout, HORIZONTAL_LEFT_ADJUST)};
        const FloatPair previousAdjust = accumulatedAdjust;
        const FloatPair previousBounds = accumulatedBounds;
        const bool baselineUpdate = OrderedLess(baselineMax, baseline);
        baselineMax = MaxSs(baseline, baselineMax);
        if (baselineUpdate) effectForBaseline = effectHeight;
        const float delta = lineAdvance - baseline;
        const bool deltaUpdate = OrderedLess(deltaMax, delta);
        deltaMax = MaxSs(delta, deltaMax);
        if (deltaUpdate) effectForDelta = effectHeight;
        const FloatPair difference{bounds.lo - adjust.hi, bounds.hi - adjust.lo};
        const FloatPair sum{previousAdjust.lo + previousBounds.lo, previousAdjust.hi + previousBounds.hi};
        if (OrderedLess(difference.lo, sum.lo)) {
            accumulatedBounds.lo = bounds.lo;
            accumulatedAdjust.lo = FlipSign(adjust.hi);
        }
        if (OrderedLess(difference.hi, sum.hi)) {
            accumulatedBounds.hi = bounds.hi;
            accumulatedAdjust.hi = adjust.lo;
        }
    }
    if (!outWords) return fail();
    StoreFloat(outWords, HORIZONTAL_BASELINE, baselineMax);
    StoreFloat(outWords, HORIZONTAL_LINE_ADVANCE, baselineMax + deltaMax);
    StoreFloat(outWords, HORIZONTAL_X_BOUND_LO, accumulatedBounds.lo);
    StoreFloat(outWords, HORIZONTAL_X_BOUND_HI, accumulatedBounds.hi);
    StoreFloat(outWords, HORIZONTAL_EFFECT_HEIGHT, MaxSs(effectForBaseline, effectForDelta));
    StoreFloat(outWords, HORIZONTAL_LEFT_ADJUST, accumulatedAdjust.lo);
    StoreFloat(outWords, HORIZONTAL_HALF_EFFECT_WIDTH, accumulatedAdjust.hi);
    return SCE_FONT_OK;
}

int Font::ComputeVerticalLayout(FontHandle handle, const StyleStateBlock* style, std::uint8_t* outWords) {
    const auto fail = [&](int rc) {
        ClearVerticalLayout(outWords);
        return rc;
    };
    auto* font = GetNativeFont(handle);
    auto* library = font ? static_cast<FontLibNative*>(font->library) : nullptr;
    if (!library || !FontContext(library, font) || !library->sys_driver) return fail(SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    const SysDriver* driver = library->sys_driver;
    const std::uint32_t modeLow = font->flags & 0x0Fu;
    const std::uint32_t fontId = font->open_info.ctx_entry_index;
    float accNegOffsetMax = 0.0f;
    float accSumMax = 0.0f;
    float spanForNegOffset = 0.0f;
    float spanForSum = 0.0f;
    float accMetrics08Max = 0.0f;
    float accMetrics0CMax = 0.0f;
    float accDiffMin = 0.0f;
    float accTempMin = 0.0f;
    if (static_cast<std::int32_t>(fontId) >= 0) {
        FontObj* head = nullptr;
        std::uint32_t lockWord = 0;
        auto* entry = AcquireFontCtxEntry(FontContext(library, font), fontId, modeLow, &head, &lockWord);
        FontObj* obj = FindSubFont(head, font->open_info.sub_font_index);
        if (!entry || !obj) {
            if (entry) ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
            return fail(SCE_FONT_ERROR_FATAL);
        }
        int rc = SCE_FONT_ERROR_FATAL;
        if ((lockWord & OPEN_BIT) != 0 && (lockWord & COUNT_MASK) != 0) {
            float scaleX = style ? style->scale_w : 0.0f;
            float scaleY = style ? style->scale_h : 0.0f;
            obj->font_handle = handle;
            obj->shift_units_x = 0;
            obj->shift_units_y = 0;
            int callRc = SCE_FONT_ERROR_FATAL;
            if (style && style->scale_unit == 0) {
                if (driver->set_char_default_dpi) callRc = driver->set_char_default_dpi(obj, scaleX, scaleY, &scaleX, &scaleY);
            } else if (driver->set_char_with_dpi) {
                callRc = driver->set_char_with_dpi(obj, style ? style->dpi_x : 0u, style ? style->dpi_y : 0u, scaleX, scaleY, &scaleX, &scaleY);
            }
            std::uint8_t layout[VERTICAL_LAYOUT_SIZE] = {};
            if (callRc == SCE_FONT_OK) {
                UpdateShiftCache(obj);
                callRc = driver->compute_layout_alt ? driver->compute_layout_alt(obj, style, layout) : SCE_FONT_ERROR_FATAL;
            }
            if (callRc == SCE_FONT_OK) {
                const float metrics00 = LoadFloat(layout, VERTICAL_COLUMN_ADVANCE);
                const float offsetCandidate = LoadFloat(layout, VERTICAL_BASELINE_OFFSET_X);
                const float metrics08 = LoadFloat(layout, VERTICAL_METRICS_0X08);
                const float metrics0C = LoadFloat(layout, VERTICAL_METRICS_0X0C);
                const float spanCandidate = LoadFloat(layout, VERTICAL_DECORATION_SPAN);
                const float extra08 = LoadFloat(layout, VERTICAL_DECORATION_0X08);
                const float extra0C = LoadFloat(layout, VERTICAL_DECORATION_0X0C);
                const float negOffset = -offsetCandidate;
                const float sum = offsetCandidate + metrics00;
                if (accNegOffsetMax < negOffset) spanForNegOffset = spanCandidate;
                accNegOffsetMax = std::max(accNegOffsetMax, negOffset);
                if (accSumMax < sum) spanForSum = spanCandidate;
                accSumMax = std::max(accSumMax, sum);
                accDiffMin = std::min(extra08 - offsetCandidate, accDiffMin);
                accMetrics08Max = std::max(metrics08, accMetrics08Max);
                accTempMin = std::min(sum + extra0C, accDiffMin);
                accMetrics0CMax = std::max(metrics0C, accMetrics0CMax);
                rc = SCE_FONT_OK;
            } else {
                rc = callRc;
            }
        }
        ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
        if (rc != SCE_FONT_OK) return fail(rc);
    }
    if (outWords) {
        StoreFloat(outWords, VERTICAL_COLUMN_ADVANCE, accNegOffsetMax + accSumMax);
        StoreFloat(outWords, VERTICAL_BASELINE_OFFSET_X, -accNegOffsetMax);
        StoreFloat(outWords, VERTICAL_METRICS_0X08, accMetrics08Max);
        StoreFloat(outWords, VERTICAL_METRICS_0X0C, accMetrics0CMax);
        StoreFloat(outWords, VERTICAL_DECORATION_SPAN, std::max(spanForSum, spanForNegOffset));
        StoreFloat(outWords, VERTICAL_DECORATION_0X08, accTempMin - accNegOffsetMax);
        StoreFloat(outWords, VERTICAL_DECORATION_0X0C, -accSumMax);
    }
    return SCE_FONT_OK;
}

int Font::GetCharGlyphMetrics(FontHandle handle, std::uint32_t code, FontGlyphMetrics* metrics, bool useCachedStyle) {
    const auto clearMetrics = [&] {
        if (metrics) *metrics = {};
    };
    auto* font = GetNativeFont(handle);
    if (!font || font->magic != HANDLE_MAGIC) {
        clearMetrics();
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    if (code == 0) {
        clearMetrics();
        return SCE_FONT_ERROR_NO_SUPPORT_CODE;
    }
    if (!metrics) return SCE_FONT_ERROR_INVALID_PARAMETER;
    std::uint32_t previousFontLock = 0;
    if (!AcquireFontLock(font, previousFontLock)) {
        clearMetrics();
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    std::uint32_t previousCachedLock = 0;
    if (useCachedStyle && !AcquireCachedStyleLock(font, previousCachedLock)) {
        ReleaseFontLock(font, previousFontLock);
        clearMetrics();
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    const auto finish = [&](int rc) {
        if (useCachedStyle) ReleaseCachedStyleLock(font, previousCachedLock);
        ReleaseFontLock(font, previousFontLock);
        if (rc != SCE_FONT_OK) clearMetrics();
        return rc;
    };
    FontState* state = TryGetState(handle);
    if (!state) return finish(SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    if (useCachedStyle && !font->renderer) return finish(SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    const StyleStateBlock* style = useCachedStyle ? &font->cached_style.state : &font->style;
    float scaleW = 0.0f;
    float scaleH = 0.0f;
    const int scaleRc = StyleStateGetScalePixel(style, &scaleW, &scaleH);
    if (scaleRc != SCE_FONT_OK) return finish(scaleRc);
    const FT_Face face = state->face;
    if (!face) return finish(SCE_FONT_ERROR_NO_SUPPORT_FUNCTION);
    const FT_UInt glyphIndex = ResolveGlyphIndexWithFallback(face, code);
    if (glyphIndex == 0) return finish(SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    const auto charW = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleW * 64.0f));
    const auto charH = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleH * 64.0f));
    FT_F26Dot6 usedW = charW;
    FT_F26Dot6 usedH = charH;
    if (SetCharSizeCompat(face, charW, charH, 72, 72, &usedW, &usedH) != 0) return finish(SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    FT_Set_Transform(face, nullptr, nullptr);
    if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT) != 0) return finish(SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    const auto ratio = [](FT_F26Dot6 requested, FT_F26Dot6 applied) {
        if (requested == 0 || applied == 0) return 1.0f;
        const double value = static_cast<double>(std::llabs(static_cast<long long>(requested))) / static_cast<double>(std::llabs(static_cast<long long>(applied)));
        return std::isfinite(value) && value > 0.0 ? static_cast<float>(value) : 1.0f;
    };
    const float ratioX = ratio(charW, usedW);
    const float ratioY = ratio(charH, usedH);
    const FT_GlyphSlot slot = face->glyph;
    metrics->width = static_cast<float>(slot->metrics.width) / 64.0f * ratioX;
    metrics->height = static_cast<float>(slot->metrics.height) / 64.0f * ratioY;
    metrics->Horizontal.bearingX = static_cast<float>(slot->metrics.horiBearingX) / 64.0f * ratioX;
    metrics->Horizontal.bearingY = static_cast<float>(slot->metrics.horiBearingY) / 64.0f * ratioY;
    metrics->Horizontal.advance = static_cast<float>(slot->metrics.horiAdvance) / 64.0f * ratioX;
    metrics->Vertical.bearingX = 0.0f;
    metrics->Vertical.bearingY = 0.0f;
    metrics->Vertical.advance = 0.0f;
    return finish(SCE_FONT_OK);
}

int Font::RenderCharGlyphImageCore(FontHandle handle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    const auto fail = [&](int rc) {
        ClearRenderOutputs(metrics, result);
        return rc;
    };
    if (!handle) return fail(SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    if (code == 0) return fail(SCE_FONT_ERROR_NO_SUPPORT_CODE);
    if (!surface || !metrics || !result) return fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    auto* font = GetNativeFont(handle);
    if (font->magic != HANDLE_MAGIC) return fail(SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    if (!font->renderer) return fail(SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    auto* library = static_cast<FontLibNative*>(font->library);
    if (!library || library->magic != LIBRARY_MAGIC) return fail(SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    const SysDriver* driver = library->sys_driver;
    if (!driver || !driver->glyph_index || !driver->set_char_with_dpi || !driver->set_char_default_dpi) return fail(SCE_FONT_ERROR_FATAL);
    const std::uint32_t modeLow = font->flags & 0x0Fu;
    StyleStateBlock surfaceStyle{};
    const StyleStateBlock* style = ResolveSurfaceStyleState(&font->cached_style.state, surface, surfaceStyle);
    FontObj* obj = nullptr;
    std::uint32_t lockWord = 0;
    auto* entry = AcquireFontCtxEntry(FontContext(library, font), font->open_info.ctx_entry_index, modeLow, &obj, &lockWord);
    obj = FindSubFont(obj, font->open_info.sub_font_index);
    if (!entry || !obj) {
        if (entry) ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
        return fail(SCE_FONT_ERROR_FATAL);
    }
    const auto finish = [&](int rc) {
        ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
        if (rc != SCE_FONT_OK) ClearRenderOutputs(metrics, result);
        return rc;
    };
    if ((lockWord & OPEN_BIT) == 0 || (lockWord & COUNT_MASK) == 0) return finish(SCE_FONT_ERROR_FATAL);
    obj->reserved_0x04 = static_cast<std::uint32_t>(font->flags >> 15);
    obj->font_handle = handle;
    obj->shift_units_x = 0;
    obj->shift_units_y = 0;
    obj->layout_seed_pair = 0;
    StyleStateGetScalePixel(style, &obj->scale_x_0x50, &obj->scale_y_0x54);
    float outScaleX = 0.0f;
    float outScaleY = 0.0f;
    int rc;
    if (style->scale_unit == 0) {
        rc = driver->set_char_default_dpi(obj, style->scale_w, style->scale_h, &outScaleX, &outScaleY);
    } else {
        rc = driver->set_char_with_dpi(obj, style->dpi_x ? style->dpi_x : 0x48, style->dpi_y ? style->dpi_y : 0x48, style->scale_w, style->scale_h, &outScaleX, &outScaleY);
    }
    if (rc != SCE_FONT_OK) return finish(SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    std::uint32_t glyphIndex = 0;
    rc = driver->glyph_index(obj, code, &glyphIndex);
    if (rc != SCE_FONT_OK || glyphIndex == 0) return finish(SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    return finish(RenderGlyphIndexToSurface(*obj, glyphIndex, surface, x, y, metrics, result));
}

bool Font::BuildTrueOutline(GeneratedGlyph& glyph) {
    FontState* state = TryGetState(glyph.owner);
    if (!state || !state->face) return false;
    const auto* font = GetNativeFont(glyph.owner);
    if (font->magic != HANDLE_MAGIC) return false;
    const FT_Face face = state->face;
    const FT_UInt glyphIndex = FT_Get_Char_Index(face, glyph.codepoint);
    if (glyphIndex == 0) return false;
    const auto charW = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(glyph.glyph.scale_x * 64.0f));
    const auto charH = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(glyph.glyph.base_scale * 64.0f));
    if (FT_Set_Char_Size(face, charW, charH, 72, 72) != 0) return false;
    FT_Set_Transform(face, nullptr, nullptr);
    if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) return false;
    const FT_GlyphSlot slot = face->glyph;
    if (!slot || slot->format != FT_GLYPH_FORMAT_OUTLINE) return false;
    const FT_Outline& outline = slot->outline;
    if (outline.n_points <= 0 || outline.n_contours <= 0 || !outline.points || !outline.tags || !outline.contours) return false;
    glyph.outlinePoints.clear();
    glyph.outlineTags.clear();
    glyph.outlineContours.clear();
    for (int i = 0; i < outline.n_points; ++i) {
        glyph.outlinePoints.push_back({static_cast<float>(outline.points[i].x) / 64.0f, static_cast<float>(outline.points[i].y) / 64.0f});
        glyph.outlineTags.push_back(FT_CURVE_TAG(outline.tags[i]) == FT_CURVE_TAG_ON ? 1 : 0);
    }
    for (int i = 0; i < outline.n_contours; ++i) glyph.outlineContours.push_back(static_cast<std::uint16_t>(outline.contours[i]));
    glyph.outline.points_ptr = glyph.outlinePoints.data();
    glyph.outline.tags_ptr = glyph.outlineTags.data();
    glyph.outline.contour_end_idx = glyph.outlineContours.data();
    glyph.outline.points_cnt = static_cast<std::int16_t>(glyph.outlinePoints.size());
    glyph.outline.contours_cnt = static_cast<std::int16_t>(glyph.outlineContours.size());
    glyph.outlineInitialized = true;
    return true;
}
