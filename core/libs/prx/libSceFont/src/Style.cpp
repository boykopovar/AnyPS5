// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

using namespace Font;

template<typename Setter>
int UpdateFontStyle(FontHandle fontHandle, Setter&& setter) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireFontLock(font, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    if (setter(font) != 0) font->cached_style.layout_cache_state = 0;
    ReleaseFontLock(font, fontLock);
    return SCE_FONT_OK;
}

template<typename Setter>
int UpdateRenderStyle(FontHandle fontHandle, Setter&& setter) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t cachedLock = 0;
    if (!font || font->magic != HANDLE_MAGIC || !AcquireCachedStyleLock(font, cachedLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    int rc = SCE_FONT_ERROR_NOT_BOUND_RENDERER;
    if (font->renderer) rc = setter(font);
    ReleaseCachedStyleLock(font, cachedLock);
    return rc;
}

template<typename Getter>
int ReadRenderStyle(FontHandle fontHandle, bool checkMagic, Getter&& getter) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t cachedLock = 0;
    if (!font || (checkMagic && font->magic != HANDLE_MAGIC) || !AcquireCachedStyleLock(font, cachedLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    const int rc = font->renderer ? getter(font) : SCE_FONT_ERROR_NOT_BOUND_RENDERER;
    ReleaseCachedStyleLock(font, cachedLock);
    return rc;
}

template<typename Getter>
int ReadFontStyle(FontHandle fontHandle, bool checkMagic, Getter&& getter) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || (checkMagic && font->magic != HANDLE_MAGIC) || !AcquireFontLock(font, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    const int rc = getter(font);
    ReleaseFontLock(font, fontLock);
    return rc;
}

template<typename Access>
int AccessFontSettings(FontHandle fontHandle, Access&& access) {
    return ReadFontStyle(fontHandle, true, [&](FontHandleNative*) {
        FontState* state = TryGetState(fontHandle);
        return state ? access(*state) : SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    });
}

bool HasTypographyRecordFeature(const void* record, int typographic) {
    const auto* bytes = static_cast<const std::byte*>(record);
    std::uint32_t count;
    std::memcpy(&count, bytes + 32, sizeof(count));
    for (std::uint32_t index = 0; index < count; ++index) {
        std::int32_t fontId;
        std::memcpy(&fontId, bytes + 36 + sizeof(fontId) * index, sizeof(fontId));
        if (count == 1) {
            if (typographic == 65538) return fontId == 43 || fontId == 12;
            return static_cast<std::uint32_t>(fontId) <= 29 && (0x30000c00u & (1u << fontId)) != 0;
        }
        if ((std::to_integer<unsigned>(bytes[100 + index]) & 0x80u) != 0 && fontId >= 0) return index == (typographic == 65538 ? 2u : 1u);
    }
    return false;
}

int CheckTypography(FontHandleNative* font, int typographic, bool setting) {
    const auto* record = font->open_info.fontset_record;
    switch (typographic) {
    case 1:
    case 17:
        return SCE_FONT_OK;
    case 65537:
        if (setting) return !record || HasTypographyRecordFeature(record, typographic) ? SCE_FONT_ERROR_INVALID_PARAMETER : SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY;
        return !record || HasTypographyRecordFeature(record, typographic) ? SCE_FONT_OK : SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY;
    case 65538:
        return !record || HasTypographyRecordFeature(record, typographic) ? SCE_FONT_OK : SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY;
    case 65539:
        return record ? SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY : SCE_FONT_OK;
    default:
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
}

bool IsScriptLanguageValid(int script, int language) {
    if (script == 0x100) return language >= 0x100 && language <= 0x105;
    if (script == 0x600) return language >= 0x600 && language <= 0x602;
    if (script != 0x3000) return false;
    switch (language) {
    case 0x3000:
    case 0x3011:
    case 0x3012:
    case 0x3013:
    case 0x3014:
    case 0x3016:
    case 0x3017:
    case 0x3018:
    case 0x3021:
    case 0x3022:
    case 0x3023:
    case 0x3041:
        return true;
    default:
        return false;
    }
}

void ResetScaleOutputs(float* w, float* h) {
    if (w) *w = 0.0f;
    if (h) *h = 0.0f;
}

void ResetDpiOutputs(std::uint32_t* hDpi, std::uint32_t* vDpi) {
    if (hDpi) *hDpi = 0;
    if (vDpi) *vDpi = 0;
}

void ResetWeightOutputs(float* weightXScale, float* weightYScale, std::uint32_t* mode) {
    if (weightXScale) *weightXScale = 1.0f;
    if (weightYScale) *weightYScale = 1.0f;
    if (mode) *mode = 0;
}

float ClampWeightDelta(float scale) {
    return std::clamp(scale - 1.0f, -0.04f, 0.04f);
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontDefineAttribute(FontHandle fontHandle, int attribute, int* oldAttribute) {
    return AccessFontSettings(fontHandle, [&](FontState& state) {
        if (oldAttribute) *oldAttribute = state.attribute;
        state.attribute = attribute;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontGetAttribute(FontHandle fontHandle, int attribute, int* nowAttribute) {
    (void)attribute;
    if (!nowAttribute) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return AccessFontSettings(fontHandle, [&](FontState& state) {
        *nowAttribute = state.attribute;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontSetScriptLanguage(FontHandle fontHandle, int fontScript, int fontLanguage) {
    const int result = AccessFontSettings(fontHandle, [&](FontState&) {
        return IsScriptLanguageValid(fontScript, fontLanguage) ? SCE_FONT_OK : SCE_FONT_ERROR_INVALID_PARAMETER;
    });
    if (result != SCE_FONT_OK) return result;
    throw std::logic_error("sceFontSetScriptLanguage: driver language selection is not implemented");
}

int APS5_VABI sceFontGetScriptLanguage(FontHandle fontHandle, int fontScript, int* fontLanguage) {
    if (fontLanguage) *fontLanguage = 0;
    const int result = AccessFontSettings(fontHandle, [&](FontState&) {
        if (!fontLanguage) return SCE_FONT_ERROR_INVALID_PARAMETER;
        return fontScript == 0x100 || fontScript == 0x600 || fontScript == 0x3000 ? SCE_FONT_OK : SCE_FONT_ERROR_INVALID_PARAMETER;
    });
    if (result != SCE_FONT_OK) return result;
    throw std::logic_error("sceFontGetScriptLanguage: driver language query is not implemented");
}

int APS5_VABI sceFontSetTypographicDesign(FontHandle fontHandle, int typographic, int feature) {
    const int result = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) {
        const int validation = CheckTypography(font, typographic, true);
        if (validation != SCE_FONT_OK) return validation;
        return feature == 0 || feature == (typographic == 1 ? 1 : 32) ? SCE_FONT_OK : SCE_FONT_ERROR_INVALID_PARAMETER;
    });
    if (result != SCE_FONT_OK) return result;
    throw std::logic_error("sceFontSetTypographicDesign: driver typography selection is not implemented");
}

int APS5_VABI sceFontGetTypographicDesign(FontHandle fontHandle, int typographic, int* feature) {
    const int result = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) {
        const int validation = CheckTypography(font, typographic, false);
        return feature ? validation : SCE_FONT_ERROR_INVALID_PARAMETER;
    });
    if (feature) *feature = 0;
    if (result != SCE_FONT_OK) return result;
    throw std::logic_error("sceFontGetTypographicDesign: driver typography query is not implemented");
}

int APS5_VABI sceFontSetScalePixel(FontHandle fontHandle, float w, float h) {
    return UpdateFontStyle(fontHandle, [&](FontHandleNative* font) { return StyleStateSetScalePixel(&font->style, w, h); });
}

int APS5_VABI sceFontSetScalePoint(FontHandle fontHandle, float w, float h) {
    return UpdateFontStyle(fontHandle, [&](FontHandleNative* font) { return StyleStateSetScalePoint(&font->style, w, h); });
}

int APS5_VABI sceFontSetResolutionDpi(FontHandle fontHandle, std::uint32_t hDpi, std::uint32_t vDpi) {
    auto* font = GetNativeFont(fontHandle);
    std::uint32_t fontLock = 0;
    if (!font || !AcquireFontLock(font, fontLock)) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    if (StyleStateSetDpi(&font->style, hDpi, vDpi) != 0) font->cached_style.layout_cache_state = 0;
    ReleaseFontLock(font, fontLock);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontSetEffectSlant(FontHandle fontHandle, float slantRatio) {
    return UpdateFontStyle(fontHandle, [&](FontHandleNative* font) { return StyleStateSetSlantRatio(&font->style, slantRatio); });
}

int APS5_VABI sceFontSetEffectWeight(FontHandle fontHandle, float weightXScale, float weightYScale, std::uint32_t mode) {
    if (mode != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return UpdateFontStyle(fontHandle, [&](FontHandleNative* font) { return StyleStateSetWeightScale(&font->style, weightXScale, weightYScale); });
}

int APS5_VABI sceFontGetScalePixel(FontHandle fontHandle, float* w, float* h) {
    const int rc = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) { return StyleStateGetScalePixel(&font->style, w, h); });
    if (rc != SCE_FONT_OK) ResetScaleOutputs(w, h);
    return rc;
}

int APS5_VABI sceFontGetScalePoint(FontHandle fontHandle, float* w, float* h) {
    const int rc = ReadFontStyle(fontHandle, false, [&](FontHandleNative* font) { return StyleStateGetScalePoint(&font->style, w, h); });
    if (rc != SCE_FONT_OK) ResetScaleOutputs(w, h);
    return rc;
}

int APS5_VABI sceFontGetResolutionDpi(FontHandle fontHandle, std::uint32_t* hDpi, std::uint32_t* vDpi) {
    const int rc = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) {
        if (!hDpi && !vDpi) return SCE_FONT_ERROR_INVALID_PARAMETER;
        if (hDpi) *hDpi = font->style.dpi_x;
        if (vDpi) *vDpi = font->style.dpi_y;
        return SCE_FONT_OK;
    });
    if (rc != SCE_FONT_OK) ResetDpiOutputs(hDpi, vDpi);
    return rc;
}

int APS5_VABI sceFontGetEffectSlant(FontHandle fontHandle, float* slantRatio) {
    const int rc = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) { return StyleStateGetSlantRatio(&font->style, slantRatio); });
    if (rc != SCE_FONT_OK && slantRatio) *slantRatio = 0.0f;
    return rc;
}

int APS5_VABI sceFontGetEffectWeight(FontHandle fontHandle, float* weightXScale, float* weightYScale, std::uint32_t* mode) {
    const int rc = ReadFontStyle(fontHandle, true, [&](FontHandleNative* font) { return StyleStateGetWeightScale(&font->style, weightXScale, weightYScale, mode); });
    if (rc != SCE_FONT_OK) ResetWeightOutputs(weightXScale, weightYScale, mode);
    return rc;
}

int APS5_VABI sceFontSetupRenderScalePixel(FontHandle fontHandle, float w, float h) {
    return UpdateRenderStyle(fontHandle, [&](FontHandleNative* font) {
        if (StyleStateSetScalePixel(&font->cached_style.state, w, h) != 0) font->cached_style.cache_flags_and_direction &= 0xFFFF0000u;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontSetupRenderScalePoint(FontHandle fontHandle, float w, float h) {
    return UpdateRenderStyle(fontHandle, [&](FontHandleNative* font) {
        if (StyleStateSetScalePoint(&font->cached_style.state, w, h) != 0) font->cached_style.cache_flags_and_direction &= 0xFFFF0000u;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontSetupRenderEffectSlant(FontHandle fontHandle, float slantRatio) {
    return UpdateRenderStyle(fontHandle, [&](FontHandleNative* font) {
        if (StyleStateSetSlantRatio(&font->cached_style.state, slantRatio) != 0) font->cached_style.cache_flags_and_direction &= 0xFFFF0000u;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontSetupRenderEffectWeight(FontHandle fontHandle, float weightXScale, float weightYScale, std::uint32_t mode) {
    return UpdateRenderStyle(fontHandle, [&](FontHandleNative* font) {
        if (mode != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
        if (StyleStateSetWeightScale(&font->cached_style.state, weightXScale, weightYScale) != 0) font->cached_style.cache_flags_and_direction &= 0xFFFF0000u;
        return SCE_FONT_OK;
    });
}

int APS5_VABI sceFontGetRenderScalePixel(FontHandle fontHandle, float* w, float* h) {
    const int rc = ReadRenderStyle(fontHandle, false, [&](FontHandleNative* font) { return StyleStateGetScalePixel(&font->cached_style.state, w, h); });
    if (rc != SCE_FONT_OK) ResetScaleOutputs(w, h);
    return rc;
}

int APS5_VABI sceFontGetRenderScalePoint(FontHandle fontHandle, float* w, float* h) {
    const int rc = ReadRenderStyle(fontHandle, false, [&](FontHandleNative* font) { return StyleStateGetScalePoint(&font->cached_style.state, w, h); });
    if (rc != SCE_FONT_OK) ResetScaleOutputs(w, h);
    return rc;
}

int APS5_VABI sceFontGetRenderEffectSlant(FontHandle fontHandle, float* slantRatio) {
    const int rc = ReadRenderStyle(fontHandle, true, [&](FontHandleNative* font) { return StyleStateGetSlantRatio(&font->cached_style.state, slantRatio); });
    if (rc != SCE_FONT_OK && slantRatio) *slantRatio = 0.0f;
    return rc;
}

int APS5_VABI sceFontGetRenderEffectWeight(FontHandle fontHandle, float* weightXScale, float* weightYScale, std::uint32_t* mode) {
    const int rc = ReadRenderStyle(fontHandle, true, [&](FontHandleNative* font) { return StyleStateGetWeightScale(&font->cached_style.state, weightXScale, weightYScale, mode); });
    if (rc != SCE_FONT_OK) ResetWeightOutputs(weightXScale, weightYScale, mode);
    return rc;
}

int APS5_VABI sceFontStyleFrameInit(FontStyleFrame* styleFrame) {
    if (!styleFrame) return SCE_FONT_ERROR_INVALID_PARAMETER;
    std::memset(&styleFrame->scaleUnit, 0, 0x20);
    std::memset(&styleFrame->layout_cache_state, 0, 0x20);
    std::memset(&styleFrame->layout_cache_bytes[0x08], 0, 0x20);
    styleFrame->magic = STYLE_FRAME_MAGIC;
    styleFrame->flags1 = 0;
    styleFrame->flags2 = 0;
    styleFrame->hDpi = 0x48;
    styleFrame->vDpi = 0x48;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameSetScalePixel(FontStyleFrame* styleFrame, float w, float h) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->scaleUnit = 0;
    styleFrame->scalePixelW = w;
    styleFrame->scalePixelH = h;
    styleFrame->flags1 |= STYLE_FRAME_FLAG_SCALE;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameSetScalePoint(FontStyleFrame* styleFrame, float w, float h) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->scaleUnit = 1;
    styleFrame->scalePixelW = w;
    styleFrame->scalePixelH = h;
    styleFrame->flags1 |= STYLE_FRAME_FLAG_SCALE;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameSetResolutionDpi(FontStyleFrame* styleFrame, std::uint32_t hDpi, std::uint32_t vDpi) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->hDpi = hDpi == 0 ? 0x48 : hDpi;
    styleFrame->vDpi = vDpi == 0 ? 0x48 : vDpi;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameSetEffectSlant(FontStyleFrame* styleFrame, float slantRatio) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->slantRatio = std::clamp(slantRatio, -1.0f, 1.0f);
    styleFrame->flags1 |= STYLE_FRAME_FLAG_SLANT;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameSetEffectWeight(FontStyleFrame* styleFrame, float weightXScale, float weightYScale, std::uint32_t mode) {
    if (!ValidStyleFrame(styleFrame) || mode != 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->effectWeightX = ClampWeightDelta(weightXScale);
    styleFrame->effectWeightY = ClampWeightDelta(weightYScale);
    styleFrame->flags1 |= STYLE_FRAME_FLAG_WEIGHT;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameUnsetScale(FontStyleFrame* styleFrame) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->flags1 &= static_cast<std::uint8_t>(~STYLE_FRAME_FLAG_SCALE);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameUnsetEffectSlant(FontStyleFrame* styleFrame) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->flags1 &= static_cast<std::uint8_t>(~STYLE_FRAME_FLAG_SLANT);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameUnsetEffectWeight(FontStyleFrame* styleFrame) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    styleFrame->flags1 &= static_cast<std::uint8_t>(~STYLE_FRAME_FLAG_WEIGHT);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameGetScalePixel(const FontStyleFrame* styleFrame, float* w, float* h) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((styleFrame->flags1 & STYLE_FRAME_FLAG_SCALE) == 0) return SCE_FONT_ERROR_UNSET_PARAMETER;
    if (!w && !h) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool points = styleFrame->scaleUnit != 0;
    if (w) *w = points && styleFrame->hDpi != 0 ? styleFrame->scalePixelW * (static_cast<float>(styleFrame->hDpi) / POINTS_PER_INCH) : styleFrame->scalePixelW;
    if (h) *h = points && styleFrame->vDpi != 0 ? styleFrame->scalePixelH * (static_cast<float>(styleFrame->vDpi) / POINTS_PER_INCH) : styleFrame->scalePixelH;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameGetScalePoint(const FontStyleFrame* styleFrame, float* w, float* h) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((styleFrame->flags1 & STYLE_FRAME_FLAG_SCALE) == 0) return SCE_FONT_ERROR_UNSET_PARAMETER;
    if (!w && !h) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool pixels = styleFrame->scaleUnit == 0;
    if (w) *w = pixels && styleFrame->hDpi != 0 ? styleFrame->scalePixelW * (POINTS_PER_INCH / static_cast<float>(styleFrame->hDpi)) : styleFrame->scalePixelW;
    if (h) *h = pixels && styleFrame->vDpi != 0 ? styleFrame->scalePixelH * (POINTS_PER_INCH / static_cast<float>(styleFrame->vDpi)) : styleFrame->scalePixelH;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameGetResolutionDpi(const FontStyleFrame* styleFrame, std::uint32_t* hDpi, std::uint32_t* vDpi) {
    if (!ValidStyleFrame(styleFrame) || (!hDpi && !vDpi)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (hDpi) *hDpi = styleFrame->hDpi;
    if (vDpi) *vDpi = styleFrame->vDpi;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameGetEffectSlant(const FontStyleFrame* styleFrame, float* slantRatio) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((styleFrame->flags1 & STYLE_FRAME_FLAG_SLANT) == 0) return SCE_FONT_ERROR_UNSET_PARAMETER;
    if (!slantRatio) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *slantRatio = styleFrame->slantRatio;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontStyleFrameGetEffectWeight(const FontStyleFrame* styleFrame, float* weightXScale, float* weightYScale, std::uint32_t* mode) {
    if (!ValidStyleFrame(styleFrame)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if ((styleFrame->flags1 & STYLE_FRAME_FLAG_WEIGHT) == 0) return SCE_FONT_ERROR_UNSET_PARAMETER;
    if (!weightXScale && !weightYScale && !mode) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (weightXScale) *weightXScale = styleFrame->effectWeightX + 1.0f;
    if (weightYScale) *weightYScale = styleFrame->effectWeightY + 1.0f;
    if (mode) *mode = 0;
    return SCE_FONT_OK;
}

}

#pragma GCC visibility pop
