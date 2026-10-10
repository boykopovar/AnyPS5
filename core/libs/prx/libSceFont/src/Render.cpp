// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "prx/libSceFont/include/FontFreeType.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using namespace Font;

struct RenderFace {
    FT_Library library = nullptr;
    FT_Face face = nullptr;

    ~RenderFace() {
        if (face) FT_Done_Face(face);
        if (library) FT_Done_FreeType(library);
    }
};

constexpr std::uint16_t GENERATE_GLYPH_DETAIL_ID = 0x0FD3;

std::uint16_t ClampToU16(float value) {
    if (value <= 0.0f) return 0;
    return static_cast<std::uint16_t>(std::lround(std::min(value, static_cast<float>(std::numeric_limits<std::uint16_t>::max()))));
}

std::uint8_t* LayoutCacheBytes(FontHandleNative* font) {
    return reinterpret_cast<std::uint8_t*>(&font->cached_style) + offsetof(CachedStyle, layout_cache_bytes);
}

RenderSurfaceSystemUse* SurfaceSystemUse(FontRenderSurface* surface) {
    return reinterpret_cast<RenderSurfaceSystemUse*>(surface->reserved_q);
}

bool SurfaceScaleFrame(FontRenderSurface* surface, StyleStateBlock& state, int& rc) {
    if (!surface || (surface->styleFlag & 0x1) == 0) return false;
    const FontStyleFrame* frame = SurfaceSystemUse(surface)->styleframe;
    if (!frame || (frame->flags1 & STYLE_FRAME_FLAG_SCALE) == 0) return false;
    state = {};
    state.dpi_x = frame->hDpi;
    state.dpi_y = frame->vDpi;
    if ((frame->flags1 & STYLE_FRAME_FLAG_SLANT) != 0) state.slant_ratio = frame->slantRatio;
    if ((frame->flags1 & STYLE_FRAME_FLAG_WEIGHT) != 0) {
        state.effect_weight_x = frame->effectWeightX;
        state.effect_weight_y = frame->effectWeightY;
    }
    StyleStateBlock frameScale{};
    frameScale.dpi_x = frame->hDpi;
    frameScale.dpi_y = frame->vDpi;
    frameScale.scale_unit = frame->scaleUnit;
    frameScale.scale_w = frame->scalePixelW;
    frameScale.scale_h = frame->scalePixelH;
    rc = StyleStateGetScalePixel(&frameScale, &state.scale_w, &state.scale_h);
    return true;
}

int CachedBaseline(FontHandle handle, FontHandleNative* font, float& baseline) {
    int rc = SCE_FONT_OK;
    const std::uint8_t flags = CachedStyleCacheFlags(font->cached_style);
    if ((flags & 0x1) == 0) {
        rc = ComputeHorizontalLayout(handle, &font->cached_style.state, LayoutCacheBytes(font));
        if (rc == SCE_FONT_OK) CachedStyleSetCacheFlags(font->cached_style, static_cast<std::uint8_t>(flags | 0x1));
    }
    if (rc == SCE_FONT_OK) baseline = LoadFloat(LayoutCacheBytes(font), HORIZONTAL_BASELINE);
    return rc;
}

int CachedColumnOffset(FontHandle handle, FontHandleNative* font, float& offset) {
    int rc = SCE_FONT_OK;
    const std::uint8_t flags = CachedStyleCacheFlags(font->cached_style);
    if ((flags & 0x2) == 0) {
        std::uint8_t layout[VERTICAL_LAYOUT_SIZE] = {};
        rc = ComputeVerticalLayout(handle, &font->cached_style.state, layout);
        if (rc == SCE_FONT_OK) {
            offset = LoadFloat(layout, VERTICAL_BASELINE_OFFSET_X);
            CachedStyleSetScalar(font->cached_style, offset);
            CachedStyleSetCacheFlags(font->cached_style, static_cast<std::uint8_t>(flags | 0x2));
        }
    }
    if (rc == SCE_FONT_OK && (CachedStyleCacheFlags(font->cached_style) & 0x2) != 0) offset = CachedStyleGetScalar(font->cached_style);
    return rc;
}

void LoadKerning(FT_Face face, float scaleW, float scaleH, std::uint32_t preCode, std::uint32_t code, FontKerning* kerning) {
    const FT_UInt previousGlyph = face ? FT_Get_Char_Index(face, preCode) : 0;
    const FT_UInt glyph = face ? FT_Get_Char_Index(face, code) : 0;
    if (!face || previousGlyph == 0 || glyph == 0) return;
    const auto charW = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleW * 64.0f));
    const auto charH = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleH * 64.0f));
    FT_Set_Char_Size(face, charW, charH, 72, 72);
    FT_Vector delta{};
    FT_Get_Kerning(face, previousGlyph, glyph, FT_KERNING_DEFAULT, &delta);
    kerning->offsetX = static_cast<float>(delta.x) / 64.0f;
}

bool FillGeneratedImageMetrics(FontRenderOutput* result, const FontGlyphMetrics* metrics, float x, float y);

int RenderGeneratedGlyph(const char* functionName, FontGlyph glyph, FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result, bool vertical) {
    ClearRenderOutputs(metrics, result);
    GlyphRenderState generated;
    if (!GetGlyphRenderState(glyph, generated)) return SCE_FONT_ERROR_INVALID_GLYPH;
    if (!renderer || static_cast<RendererNative*>(renderer)->magic != RENDERER_MAGIC) return SCE_FONT_ERROR_INVALID_RENDERER;
    auto* nativeRenderer = static_cast<RendererNative*>(renderer);
    const auto* selection = static_cast<const RendererSelection*>(nativeRenderer->selection);
    if (!selection || selection->magic != 0 || selection->size != sizeof(RendererFt)) NotImplemented_nid_no_patch(functionName);
    auto* rendererFt = static_cast<RendererFt*>(renderer);
    if (rendererFt->ft_backend.initialized_marker != rendererFt || rendererFt->ft_backend.renderer_header_0x10 != &nativeRenderer->mem_kind) NotImplemented_nid_no_patch(functionName);
    if (styleFrame && !ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((generated.flags & ~0x11u) != 0 || (generated.glyphForm != 0 && generated.glyphForm != 1) ||
        (generated.glyphForm == 0 && generated.metricsForm != 0) || (generated.glyphForm == 1 && (generated.metricsForm == 0 || generated.metricsForm > 4))) NotImplemented_nid_no_patch(functionName);
    if ((generated.flags & 0x11u) != 0) NotImplemented_nid_no_patch(functionName);
    if (generated.unsupportedCreationEffects) NotImplemented_nid_no_patch(functionName);
    if (styleFrame && ((styleFrame->flags1 & static_cast<std::uint8_t>(~STYLE_FRAME_FLAG_SCALE)) != 0 || styleFrame->flags2 != 0)) NotImplemented_nid_no_patch(functionName);
    if (!surface || !metrics || !result || !std::isfinite(x) || !std::isfinite(y) ||
        static_cast<double>(x) < std::numeric_limits<std::int32_t>::min() || static_cast<double>(x) > std::numeric_limits<std::int32_t>::max() ||
        static_cast<double>(y) < std::numeric_limits<std::int32_t>::min() || static_cast<double>(y) > std::numeric_limits<std::int32_t>::max()) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!surface->buffer || surface->width <= 0 || surface->height <= 0 || surface->widthByte <= 0 || surface->pixelSizeByte <= 0) return SCE_FONT_ERROR_NO_SUPPORT_SURFACE;
    const int bytesPerPixel = surface->pixelSizeByte;
    if ((bytesPerPixel != 1 && bytesPerPixel != 4) || static_cast<std::int64_t>(surface->widthByte) < static_cast<std::int64_t>(surface->width) * bytesPerPixel) return SCE_FONT_ERROR_NO_SUPPORT_SURFACE;
    if (!generated.faceData || generated.faceData->empty() || generated.glyphIndex == 0) return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;

    float scaleW = generated.scaleW;
    float scaleH = generated.scaleH;
    if (styleFrame && (styleFrame->flags1 & STYLE_FRAME_FLAG_SCALE) != 0) {
        if (styleFrame->scaleUnit > 1) NotImplemented_nid_no_patch(functionName);
        StyleStateBlock state{};
        state.dpi_x = styleFrame->hDpi ? styleFrame->hDpi : 72;
        state.dpi_y = styleFrame->vDpi ? styleFrame->vDpi : 72;
        state.scale_unit = styleFrame->scaleUnit;
        state.scale_w = styleFrame->scalePixelW;
        state.scale_h = styleFrame->scalePixelH;
        if (StyleStateGetScalePixel(&state, &scaleW, &scaleH) != SCE_FONT_OK) return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    if (!std::isfinite(scaleW) || !std::isfinite(scaleH) || scaleW <= 0.0f || scaleH <= 0.0f ||
        static_cast<double>(scaleW) * 64.0 >= static_cast<double>(std::numeric_limits<FT_F26Dot6>::max()) ||
        static_cast<double>(scaleH) * 64.0 >= static_cast<double>(std::numeric_limits<FT_F26Dot6>::max())) return SCE_FONT_ERROR_INVALID_PARAMETER;

    RenderFace renderFace;
    if (FT_Init_FreeType(&renderFace.library) != 0 || !renderFace.library || generated.faceData->size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) ||
        FT_New_Memory_Face(renderFace.library, generated.faceData->data(), static_cast<FT_Long>(generated.faceData->size()), static_cast<FT_Long>(generated.faceIndex), &renderFace.face) != 0 || !renderFace.face) return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    FT_Face face = renderFace.face;
    const auto charW = static_cast<FT_F26Dot6>(scaleW * 64.0f);
    const auto charH = static_cast<FT_F26Dot6>(scaleH * 64.0f);
    if (FT_Set_Char_Size(face, charW, charH, 72, 72) != 0 || FT_Load_Glyph(face, generated.glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT) != 0) {
        return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    }
    FT_GlyphSlot slot = face->glyph;
    metrics->width = static_cast<float>(slot->metrics.width) / 64.0f;
    metrics->height = static_cast<float>(slot->metrics.height) / 64.0f;
    metrics->Horizontal.bearingX = static_cast<float>(slot->metrics.horiBearingX) / 64.0f;
    metrics->Horizontal.bearingY = static_cast<float>(slot->metrics.horiBearingY) / 64.0f;
    metrics->Horizontal.advance = static_cast<float>(slot->metrics.horiAdvance) / 64.0f;
    metrics->Vertical.bearingX = static_cast<float>(slot->metrics.vertBearingX) / 64.0f;
    metrics->Vertical.bearingY = static_cast<float>(slot->metrics.vertBearingY) / 64.0f;
    metrics->Vertical.advance = static_cast<float>(slot->metrics.vertAdvance) / 64.0f;
    const float verticalShiftX = metrics->Vertical.bearingX - metrics->Horizontal.bearingX;
    const float verticalShiftY = metrics->Horizontal.bearingY + metrics->Vertical.bearingY;
    const FT_Int32 loadFlags = FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT;
    FT_Vector delta{};
    delta.x = static_cast<FT_Pos>(static_cast<std::int32_t>((x - std::floor(x)) * 64.0f));
    delta.y = static_cast<FT_Pos>(-static_cast<std::int32_t>((y - std::floor(y)) * 64.0f));
    if (vertical) {
        if (!std::isfinite(verticalShiftX) || !std::isfinite(verticalShiftY) || std::abs(static_cast<double>(verticalShiftX) * 64.0) > static_cast<double>(std::numeric_limits<FT_Pos>::max()) - 64.0 || std::abs(static_cast<double>(verticalShiftY) * 64.0) > static_cast<double>(std::numeric_limits<FT_Pos>::max()) - 64.0) {
            ClearRenderOutputs(metrics, result);
            return SCE_FONT_ERROR_INVALID_PARAMETER;
        }
        delta.x += static_cast<FT_Pos>(verticalShiftX * 64.0f);
        delta.y -= static_cast<FT_Pos>(verticalShiftY * 64.0f);
    }
    FT_Set_Transform(face, nullptr, &delta);
    const FT_Error renderError = FT_Load_Glyph(face, generated.glyphIndex, loadFlags | FT_LOAD_RENDER);
    FT_Set_Transform(face, nullptr, nullptr);
    if (renderError != 0 || !face->glyph) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    }
    slot = face->glyph;
    const int glyphW = static_cast<int>(slot->bitmap.width);
    const int glyphH = static_cast<int>(slot->bitmap.rows);
    FontGlyphMetrics imageMetrics = *metrics;
    if (vertical) {
        imageMetrics.Horizontal.bearingX = metrics->Vertical.bearingX;
        imageMetrics.Horizontal.bearingY = metrics->Vertical.bearingY;
        imageMetrics.Horizontal.advance = metrics->Vertical.advance;
    }
    if (!FillGeneratedImageMetrics(result, &imageMetrics, x, y)) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    result->ImageMetrics.width = static_cast<std::uint32_t>(glyphW);
    result->ImageMetrics.height = static_cast<std::uint32_t>(glyphH);
    std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(glyphW) * static_cast<std::size_t>(glyphH));
    for (int row = 0; row < glyphH; ++row) {
        const int sourceRowIndex = slot->bitmap.pitch >= 0 ? row : glyphH - 1 - row;
        const auto* sourceRow = slot->bitmap.buffer + static_cast<std::ptrdiff_t>(sourceRowIndex) * std::abs(slot->bitmap.pitch);
        auto* targetRow = bitmap.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(glyphW);
        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
            for (int column = 0; column < glyphW; ++column) targetRow[column] = (sourceRow[column >> 3] & (0x80u >> (column & 7))) ? 0xFF : 0x00;
        } else if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            std::memcpy(targetRow, sourceRow, static_cast<std::size_t>(glyphW));
        } else if (glyphW > 0) {
            ClearRenderOutputs(metrics, result);
            return SCE_FONT_ERROR_NO_SUPPORT_SURFACE;
        }
    }
    const double destXValue = std::floor(static_cast<double>(x)) + slot->bitmap_left;
    const double destYValue = std::floor(static_cast<double>(y)) - slot->bitmap_top;
    if (destXValue < static_cast<double>(std::numeric_limits<std::int32_t>::min()) || destXValue > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
        destYValue < static_cast<double>(std::numeric_limits<std::int32_t>::min()) || destYValue > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    const std::int64_t destX = static_cast<std::int64_t>(std::floor(destXValue));
    const std::int64_t destY = static_cast<std::int64_t>(std::floor(destYValue));
    const int clipX0 = static_cast<int>(std::min(surface->sc_x0, static_cast<std::uint32_t>(surface->width)));
    const int clipY0 = static_cast<int>(std::min(surface->sc_y0, static_cast<std::uint32_t>(surface->height)));
    const int clipX1 = std::max(clipX0, static_cast<int>(std::min(surface->sc_x1, static_cast<std::uint32_t>(surface->width))));
    const int clipY1 = std::max(clipY0, static_cast<int>(std::min(surface->sc_y1, static_cast<std::uint32_t>(surface->height))));
    const int startX = static_cast<int>(std::clamp<std::int64_t>(destX, clipX0, clipX1));
    const int startY = static_cast<int>(std::clamp<std::int64_t>(destY, clipY0, clipY1));
    const int endX = static_cast<int>(std::clamp<std::int64_t>(destX + glyphW, clipX0, clipX1));
    const int endY = static_cast<int>(std::clamp<std::int64_t>(destY + glyphH, clipY0, clipY1));
    for (int row = startY; row < endY; ++row) {
        const auto* sourceRow = bitmap.data() + static_cast<std::size_t>(row - destY) * static_cast<std::size_t>(glyphW);
        auto* targetRow = static_cast<std::uint8_t*>(surface->buffer) + static_cast<std::size_t>(row) * static_cast<std::size_t>(surface->widthByte);
        for (int column = startX; column < endX; ++column) {
            const std::uint8_t coverage = sourceRow[column - destX];
            std::memset(targetRow + static_cast<std::size_t>(column) * static_cast<std::size_t>(bytesPerPixel), coverage, static_cast<std::size_t>(bytesPerPixel));
        }
    }
    result->stage = nullptr;
    result->SurfaceImage.address = endX > startX && endY > startY ? static_cast<std::uint8_t*>(surface->buffer) + static_cast<std::size_t>(startY) * static_cast<std::size_t>(surface->widthByte) + static_cast<std::size_t>(startX) * static_cast<std::size_t>(bytesPerPixel) : nullptr;
    result->SurfaceImage.widthByte = static_cast<std::uint32_t>(surface->widthByte);
    result->SurfaceImage.pixelSizeByte = static_cast<std::uint8_t>(bytesPerPixel);
    result->SurfaceImage.pixelFormat = 0;
    result->UpdateRect.x = static_cast<std::uint32_t>(startX);
    result->UpdateRect.y = static_cast<std::uint32_t>(startY);
    result->UpdateRect.w = static_cast<std::uint32_t>(std::max(0, endX - startX));
    result->UpdateRect.h = static_cast<std::uint32_t>(std::max(0, endY - startY));
    return SCE_FONT_OK;
}

int RenderDirectional(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result, std::uint16_t direction) {
    auto* font = GetNativeFont(fontHandle);
    if (!font || font->magic != HANDLE_MAGIC) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    if (code == 0) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_NO_SUPPORT_CODE;
    }
    if (!surface || !metrics || !result) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    std::uint32_t fontLock = 0;
    if (!AcquireFontLock(font, fontLock)) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    CachedStyleSetDirectionWord(font->cached_style, direction);
    const int rc = RenderCharGlyphImageCore(fontHandle, code, surface, x, y, metrics, result);
    ReleaseFontLock(font, fontLock);
    if (rc != SCE_FONT_OK) ClearRenderOutputs(metrics, result);
    return rc;
}

bool FillGeneratedImageMetrics(FontRenderOutput* result, const FontGlyphMetrics* metrics, float x, float y) {
    const double leftF = static_cast<double>(x) + metrics->Horizontal.bearingX;
    const double topF = static_cast<double>(y) + metrics->Horizontal.bearingY;
    const double rightF = leftF + metrics->width;
    const double bottomF = topF - metrics->height;
    const double advanceF = static_cast<double>(x) + metrics->Horizontal.advance;
    if (!std::isfinite(leftF) || !std::isfinite(topF) || !std::isfinite(rightF) || !std::isfinite(bottomF) || !std::isfinite(advanceF)) return false;
    constexpr double minimum = static_cast<double>(std::numeric_limits<std::int32_t>::min() + 1);
    constexpr double maximum = static_cast<double>(std::numeric_limits<std::int32_t>::max() - 1);
    if (leftF < minimum || leftF > maximum || topF < minimum || topF > maximum || rightF < minimum || rightF > maximum ||
        bottomF < minimum || bottomF > maximum || advanceF < minimum || advanceF > maximum) return false;
    const auto left = static_cast<std::int64_t>(std::floor(leftF));
    const auto top = static_cast<std::int64_t>(std::ceil(topF));
    const auto right = static_cast<std::int64_t>(std::ceil(rightF));
    const auto bottom = static_cast<std::int64_t>(std::floor(bottomF));
    const auto advance = static_cast<std::int64_t>(std::ceil(advanceF));
    result->ImageMetrics.bearingX = static_cast<float>(left) - x;
    result->ImageMetrics.bearingY = static_cast<float>(top) - y;
    result->ImageMetrics.advance = static_cast<float>(advance) - x;
    std::int64_t stride = advance;
    if (advanceF != static_cast<double>(advance)) {
        const auto candidate = static_cast<std::int64_t>((static_cast<double>(right) - rightF) + advanceF);
        stride = right + 1;
        if (advanceF <= rightF) stride = candidate;
        if (stride < candidate) stride = candidate;
    }
    result->ImageMetrics.stride = static_cast<float>(stride) - x;
    result->ImageMetrics.width = static_cast<std::uint32_t>(std::max<std::int64_t>(0, right - left));
    result->ImageMetrics.height = static_cast<std::uint32_t>(std::max<std::int64_t>(0, top - bottom));
    return true;
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontGetCharGlyphMetrics(FontHandle fontHandle, std::uint32_t code, FontGlyphMetrics* metrics) {
    return GetCharGlyphMetrics(fontHandle, code, metrics, false);
}

int APS5_VABI sceFontGetRenderCharGlyphMetrics(FontHandle fontHandle, std::uint32_t code, FontGlyphMetrics* metrics) {
    return GetCharGlyphMetrics(fontHandle, code, metrics, true);
}

int APS5_VABI sceFontGetHorizontalLayout(FontHandle fontHandle, FontHorizontalLayout* layout) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireFontLock(font, fontLock)) {
        if (layout) *layout = {};
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    if (!layout) {
        ReleaseFontLock(font, fontLock);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    std::uint8_t blocks[HORIZONTAL_LAYOUT_SIZE] = {};
    const int rc = ComputeHorizontalLayout(fontHandle, &font->style, blocks);
    ReleaseFontLock(font, fontLock);
    if (rc != SCE_FONT_OK) {
        *layout = {};
        return rc;
    }
    layout->baselineOffset = LoadFloat(blocks, HORIZONTAL_BASELINE);
    layout->lineAdvance = LoadFloat(blocks, HORIZONTAL_LINE_ADVANCE);
    layout->decorationExtent = LoadFloat(blocks, HORIZONTAL_EFFECT_HEIGHT);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGetVerticalLayout(FontHandle fontHandle, FontVerticalLayout* layout) {
    int rc = SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (font && font->magic == HANDLE_MAGIC && AcquireFontLock(font, fontLock)) {
        if (!layout) {
            rc = SCE_FONT_ERROR_INVALID_PARAMETER;
        } else {
            std::uint8_t blocks[VERTICAL_LAYOUT_SIZE] = {};
            rc = ComputeVerticalLayout(fontHandle, &font->style, blocks);
            if (rc == SCE_FONT_OK) {
                layout->baselineOffsetX = LoadFloat(blocks, VERTICAL_BASELINE_OFFSET_X);
                layout->columnAdvance = LoadFloat(blocks, VERTICAL_COLUMN_ADVANCE);
                layout->decorationSpan = LoadFloat(blocks, VERTICAL_DECORATION_SPAN);
            }
        }
        ReleaseFontLock(font, fontLock);
        if (rc == SCE_FONT_OK) return rc;
    }
    if (layout) *layout = {};
    return rc;
}

int APS5_VABI sceFontGetKerning(FontHandle fontHandle, std::uint32_t preCode, std::uint32_t code, FontKerning* kerning) {
    if (!kerning) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireFontLock(font, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    *kerning = {};
    const FontState* state = TryGetState(fontHandle);
    if (state) LoadKerning(state->face, state->scaleW, state->scaleH, preCode, code, kerning);
    ReleaseFontLock(font, fontLock);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGetRenderScaledKerning(FontHandle fontHandle, std::uint32_t preCode, std::uint32_t code, FontKerning* kerning) {
    if (!kerning) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *kerning = {};
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireFontLock(font, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    std::uint32_t cachedLock = 0;
    if (!AcquireCachedStyleLock(font, cachedLock)) {
        ReleaseFontLock(font, fontLock);
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    int rc = SCE_FONT_ERROR_NOT_BOUND_RENDERER;
    if (font->renderer) {
        float scaleW = 0.0f;
        float scaleH = 0.0f;
        rc = StyleStateGetScalePixel(&font->cached_style.state, &scaleW, &scaleH);
        const FontState* state = TryGetState(fontHandle);
        if (rc == SCE_FONT_OK && state) LoadKerning(state->face, scaleW, scaleH, preCode, code, kerning);
    }
    ReleaseCachedStyleLock(font, cachedLock);
    ReleaseFontLock(font, fontLock);
    return rc;
}

int APS5_VABI sceFontRenderCharGlyphImage(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireFontLock(font, fontLock)) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    }
    int preRc = SCE_FONT_OK;
    StyleStateBlock frameState{};
    const bool horizontal = static_cast<std::int16_t>(font->flags) >= 0;
    float xUsed = x;
    float yUsed = y;
    if (horizontal) {
        float baseline = 0.0f;
        if (!SurfaceScaleFrame(surface, frameState, preRc)) {
            preRc = CachedBaseline(fontHandle, font, baseline);
        } else {
            std::uint8_t layout[HORIZONTAL_LAYOUT_SIZE] = {};
            if (preRc == SCE_FONT_OK) preRc = ComputeHorizontalLayout(fontHandle, &frameState, layout);
            if (preRc == SCE_FONT_OK) {
                baseline = LoadFloat(layout, HORIZONTAL_BASELINE);
                SurfaceSystemUse(surface)->catchedScale = baseline;
            }
        }
        yUsed = y + baseline;
        CachedStyleSetDirectionWord(font->cached_style, 1);
    } else {
        float offset = 0.0f;
        if (!SurfaceScaleFrame(surface, frameState, preRc)) {
            preRc = CachedColumnOffset(fontHandle, font, offset);
        } else {
            std::uint8_t layout[VERTICAL_LAYOUT_SIZE] = {};
            if (preRc == SCE_FONT_OK) preRc = ComputeVerticalLayout(fontHandle, &frameState, layout);
            if (preRc == SCE_FONT_OK) {
                offset = LoadFloat(layout, VERTICAL_BASELINE_OFFSET_X);
                SurfaceSystemUse(surface)->catchedScale = offset;
            } else {
                preRc = CachedColumnOffset(fontHandle, font, offset);
            }
        }
        xUsed = x + offset;
        CachedStyleSetDirectionWord(font->cached_style, 2);
    }
    int rc;
    if (code == 0) {
        rc = SCE_FONT_ERROR_NO_SUPPORT_CODE;
    } else if (!surface || !metrics || !result) {
        rc = SCE_FONT_ERROR_INVALID_PARAMETER;
    } else if (preRc != SCE_FONT_OK) {
        rc = preRc;
    } else {
        rc = RenderCharGlyphImageCore(fontHandle, code, surface, xUsed, yUsed, metrics, result);
    }
    ReleaseFontLock(font, fontLock);
    if (rc != SCE_FONT_OK) ClearRenderOutputs(metrics, result);
    return rc;
}

int APS5_VABI sceFontRenderCharGlyphImageHorizontal(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    return RenderDirectional(fontHandle, code, surface, x, y, metrics, result, 1);
}

int APS5_VABI sceFontRenderCharGlyphImageVertical(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    return RenderDirectional(fontHandle, code, surface, x, y, metrics, result, 2);
}

int APS5_VABI sceFontGlyphRenderImage(FontGlyph glyph, FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    if (styleFrame && !ValidStyleFrame(styleFrame)) {
        ClearRenderOutputs(metrics, result);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    if (styleFrame) {
        ClearRenderOutputs(metrics, result);
        NotImplemented_nid_no_patch(__func__);
    }
    return RenderGeneratedGlyph(__func__, glyph, styleFrame, renderer, surface, x, y, metrics, result, false);
}

int APS5_VABI sceFontGlyphRenderImageHorizontal(FontGlyph glyph, FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    return RenderGeneratedGlyph(__func__, glyph, styleFrame, renderer, surface, x, y, metrics, result, false);
}

int APS5_VABI sceFontGlyphRenderImageVertical(FontGlyph glyph, FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result) {
    return RenderGeneratedGlyph(__func__, glyph, styleFrame, renderer, surface, x, y, metrics, result, true);
}

void APS5_VABI sceFontRenderSurfaceInit(FontRenderSurface* renderSurface, void* buffer, int bufWidthByte, int pixelSizeByte, int widthPixel, int heightPixel) {
    if (!renderSurface) return;
    const auto width = static_cast<std::uint32_t>(std::max(widthPixel, 0));
    const auto height = static_cast<std::uint32_t>(std::max(heightPixel, 0));
    renderSurface->buffer = buffer;
    renderSurface->widthByte = bufWidthByte;
    renderSurface->pixelSizeByte = static_cast<std::int8_t>(pixelSizeByte);
    renderSurface->pad0 = 0;
    renderSurface->styleFlag = 0;
    renderSurface->pad2 = 0;
    renderSurface->width = static_cast<std::int32_t>(width);
    renderSurface->height = static_cast<std::int32_t>(height);
    renderSurface->sc_x0 = 0;
    renderSurface->sc_y0 = 0;
    renderSurface->sc_x1 = width;
    renderSurface->sc_y1 = height;
}

void APS5_VABI sceFontRenderSurfaceSetScissor(FontRenderSurface* renderSurface, int x0, int y0, int w, int h) {
    if (!renderSurface) return;
    const auto surfaceW = static_cast<std::uint32_t>(renderSurface->width);
    if (surfaceW != 0) {
        std::uint32_t x1;
        std::uint32_t left;
        auto width = static_cast<std::uint32_t>(w);
        if (x0 < 0) {
            x1 = width + static_cast<std::uint32_t>(x0);
            if (surfaceW < x1) x1 = surfaceW;
            if (width <= static_cast<std::uint32_t>(-x0)) x1 = 0;
            left = 0;
        } else {
            x1 = surfaceW;
            left = surfaceW;
            if (static_cast<std::uint32_t>(x0) <= surfaceW) {
                if (surfaceW < width) width = surfaceW;
                x1 = width + static_cast<std::uint32_t>(x0);
                left = static_cast<std::uint32_t>(x0);
                if (surfaceW < x1) x1 = surfaceW;
            }
        }
        renderSurface->sc_x0 = left;
        renderSurface->sc_x1 = x1;
    }
    const auto surfaceH = static_cast<std::uint32_t>(renderSurface->height);
    if (surfaceH == 0) return;
    std::uint32_t top;
    std::uint32_t y1 = surfaceH;
    auto height = static_cast<std::uint32_t>(h);
    if (y0 < 0) {
        top = 0;
        if (height <= static_cast<std::uint32_t>(-y0)) {
            renderSurface->sc_y0 = 0;
            renderSurface->sc_y1 = 0;
            return;
        }
    } else {
        if (surfaceH < static_cast<std::uint32_t>(y0)) {
            renderSurface->sc_y0 = surfaceH;
            renderSurface->sc_y1 = y1;
            return;
        }
        top = static_cast<std::uint32_t>(y0);
        if (surfaceH < height) height = surfaceH;
    }
    const std::uint32_t candidate = height + static_cast<std::uint32_t>(y0);
    if (candidate <= surfaceH) y1 = candidate;
    renderSurface->sc_y0 = top;
    renderSurface->sc_y1 = y1;
}

int APS5_VABI sceFontRenderSurfaceSetStyleFrame(FontRenderSurface* renderSurface, FontStyleFrame* styleFrame) {
    if (!renderSurface) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!styleFrame) {
        renderSurface->styleFlag &= static_cast<std::uint8_t>(~0x1u);
        renderSurface->reserved_q[0] = 0;
        renderSurface->reserved_q[1] = 0;
        return SCE_FONT_OK;
    }
    if (styleFrame->magic != STYLE_FRAME_MAGIC) return SCE_FONT_ERROR_INVALID_PARAMETER;
    renderSurface->styleFlag |= 0x1;
    renderSurface->reserved_q[0] = reinterpret_cast<std::uint64_t>(styleFrame);
    renderSurface->reserved_q[1] = 0;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGenerateCharGlyph(FontHandle fontHandle, std::uint32_t code, const FontGenerateGlyphDetail* detail, FontGlyph* pGlyph) {
    if (!pGlyph) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *pGlyph = nullptr;
    if (!fontHandle) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    if (code == 0) return SCE_FONT_ERROR_NO_SUPPORT_CODE;
    const FontState* state = TryGetState(fontHandle);
    if (!state) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    const std::uint8_t glyphForm = detail ? detail->glyph_form : 0;
    const std::uint8_t metricsForm = detail ? detail->metrics_form : 0;
    const std::uint16_t formOptions = detail ? detail->form_options : 0;
    const FontMemory* glyphMemory = detail ? detail->mem : nullptr;
    if (detail && detail->id != GENERATE_GLYPH_DETAIL_ID) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((formOptions & static_cast<std::uint16_t>(~0x11u)) != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (glyphForm == 0 && metricsForm != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((glyphForm != 0 && metricsForm == 0) || glyphForm > 1 || metricsForm > 4) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (glyphMemory && glyphMemory->mem_kind != MEMORY_MAGIC) return SCE_FONT_ERROR_INVALID_PARAMETER;
    FontGlyphMetrics metrics{};
    const int rc = GetCharGlyphMetrics(fontHandle, code, &metrics, false);
    if (rc != SCE_FONT_OK) return rc;
    float scaleW = 0.0f;
    float scaleH = 0.0f;
    auto* nativeFont = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!nativeFont || !AcquireFontLock(nativeFont, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    const int scaleRc = StyleStateGetScalePixel(&nativeFont->style, &scaleW, &scaleH);
    const bool unsupportedCreationEffects = nativeFont->style.slant_ratio != 0.0f ||
        nativeFont->style.effect_weight_x != 0.0f || nativeFont->style.effect_weight_y != 0.0f;
    ReleaseFontLock(nativeFont, fontLock);
    if (scaleRc != SCE_FONT_OK) return scaleRc;
    auto* generated = new (std::nothrow) GeneratedGlyph();
    if (!generated) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    generated->codepoint = code;
    generated->glyphIndex = ResolveGlyphIndexWithFallback(state->face, code);
    if (generated->glyphIndex == 0) {
        delete generated;
        return SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    }
    generated->faceIndex = static_cast<std::uint32_t>(state->face->face_index);
    generated->faceData = state->faceData;
    generated->unsupportedCreationEffects = unsupportedCreationEffects;
    generated->metrics = metrics;
    generated->glyph.magic = GLYPH_MAGIC;
    generated->glyph.flags = formOptions;
    generated->glyph.glyph_form = glyphForm;
    generated->glyph.metrics_form = metricsForm;
    generated->glyph.em_size = ClampToU16(scaleH);
    generated->glyph.baseline = ClampToU16(metrics.Horizontal.bearingY);
    generated->glyph.height_px = ClampToU16(metrics.height);
    generated->glyph.origin_x = 0;
    generated->glyph.origin_y = generated->glyph.baseline;
    generated->glyph.scale_x = scaleW;
    generated->glyph.base_scale = scaleH;
    generated->glyph.memory = glyphMemory;
    generated->owner = fontHandle;
    generated->outline.outline_flags = generated->glyph.flags;
    TrackGeneratedGlyph(&generated->glyph);
    *pGlyph = &generated->glyph;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontDeleteGlyph(const FontMemory* memory, FontGlyph* pGlyph) {
    (void)memory;
    if (!pGlyph) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const FontGlyph glyph = *pGlyph;
    if (!glyph || glyph->magic != GLYPH_MAGIC || !ForgetGeneratedGlyph(glyph)) return SCE_FONT_ERROR_INVALID_GLYPH;
    delete reinterpret_cast<GeneratedGlyph*>(glyph);
    *pGlyph = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGlyphDefineAttribute(FontGlyph glyph, std::uint32_t attribute, std::uint64_t value) {
    (void)attribute;
    (void)value;
    if (!glyph || glyph->magic != GLYPH_MAGIC) return SCE_FONT_ERROR_INVALID_GLYPH;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGlyphGetGlyphForm(FontGlyph glyph) {
    if (!glyph || glyph->magic != GLYPH_MAGIC) return SCE_FONT_ERROR_INVALID_GLYPH;
    return glyph->glyph_form;
}

int APS5_VABI sceFontGlyphGetMetricsForm(FontGlyph glyph) {
    if (!glyph || glyph->magic != GLYPH_MAGIC) return SCE_FONT_ERROR_INVALID_GLYPH;
    return glyph->metrics_form;
}

int APS5_VABI sceFontGlyphGetScalePixel(FontGlyph glyph, float* w, float* h) {
    if (!glyph || glyph->magic != GLYPH_MAGIC || (!w && !h)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (w) *w = glyph->scale_x;
    if (h) *h = glyph->base_scale;
    return SCE_FONT_OK;
}

const FontGlyphMetrics* APS5_VABI sceFontGlyphRefersMetrics(FontGlyph glyph) {
    auto* generated = TryGetGeneratedGlyph(glyph);
    return generated ? &generated->metrics : nullptr;
}

const FontGlyphMetricsHorizontal* APS5_VABI sceFontGlyphRefersMetricsHorizontal(FontGlyph glyph) {
    auto* generated = TryGetGeneratedGlyph(glyph);
    if (!generated) return nullptr;
    PopulateGlyphMetricVariants(*generated);
    return &generated->metricsHorizontal;
}

const FontGlyphMetricsHorizontalAdvance* APS5_VABI sceFontGlyphRefersMetricsHorizontalAdvance(FontGlyph glyph) {
    auto* generated = TryGetGeneratedGlyph(glyph);
    if (!generated) return nullptr;
    PopulateGlyphMetricVariants(*generated);
    return &generated->metricsHorizontalAdvance;
}

const FontGlyphMetricsHorizontalX* APS5_VABI sceFontGlyphRefersMetricsHorizontalX(FontGlyph glyph) {
    auto* generated = TryGetGeneratedGlyph(glyph);
    if (!generated) return nullptr;
    PopulateGlyphMetricVariants(*generated);
    return &generated->metricsHorizontalX;
}

FontGlyphOutline* APS5_VABI sceFontGlyphRefersOutline(FontGlyph glyph) {
    if (!glyph || glyph->magic != GLYPH_MAGIC || glyph->glyph_form != 1) return nullptr;
    auto* generated = TryGetGeneratedGlyph(glyph);
    if (!generated) return nullptr;
    if (!generated->outlineInitialized && !BuildTrueOutline(*generated)) BuildBoundingOutline(*generated);
    return &generated->outline;
}

}

#pragma GCC visibility pop
