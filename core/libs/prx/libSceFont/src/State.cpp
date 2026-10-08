// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

std::mutex stateMutex;
std::unordered_map<FontHandle, std::unique_ptr<Font::FontState>> fontStates;
std::mutex glyphMutex;
std::unordered_set<FontGlyph> generatedGlyphs;

FT_Library HostFreeTypeLibrary() {
    static const FT_Library library = [] {
        FT_Library created = nullptr;
        if (FT_Init_FreeType(&created) != 0) return static_cast<FT_Library>(nullptr);
        return created;
    }();
    return library;
}

bool AcquireMagicLock(std::uint32_t& word, const std::uint16_t& magic, std::uint16_t expectedMagic, std::uint32_t& previous) {
    for (;;) {
        const std::uint32_t current = std::atomic_ref<std::uint32_t>(word).load(std::memory_order_acquire);
        if (magic != expectedMagic) return false;
        if (static_cast<std::int32_t>(current) >= 0) {
            std::uint32_t expected = current;
            if (std::atomic_ref<std::uint32_t>(word).compare_exchange_weak(expected, current | Font::LOCK_BIT, std::memory_order_acq_rel)) {
                previous = current;
                return true;
            }
        }
        Font::Backoff();
    }
}

void ReleaseWord(std::uint32_t& word, std::uint32_t previous) {
    std::atomic_ref<std::uint32_t>(word).store(previous & ~Font::LOCK_BIT, std::memory_order_release);
}

}

Font::FontState::~FontState() {
    if (face) FT_Done_Face(face);
}

void Font::Backoff() {
    PreciseSleepUs(30);
}

std::uint32_t Font::AcquireWordLock(std::uint32_t& word) {
    for (;;) {
        const std::uint32_t current = std::atomic_ref<std::uint32_t>(word).load(std::memory_order_acquire);
        if (static_cast<std::int32_t>(current) >= 0) {
            std::uint32_t expected = current;
            if (std::atomic_ref<std::uint32_t>(word).compare_exchange_weak(expected, current | LOCK_BIT, std::memory_order_acq_rel)) return current;
        }
        Backoff();
    }
}

Font::FontHandleNative* Font::GetNativeFont(FontHandle handle) {
    return reinterpret_cast<FontHandleNative*>(handle);
}

bool Font::AcquireLibraryLock(FontLibNative* library, std::uint32_t& previous) {
    return library && AcquireMagicLock(library->lock_word, library->magic, LIBRARY_MAGIC, previous);
}

void Font::ReleaseLibraryLock(FontLibNative* library, std::uint32_t previous) {
    if (library) ReleaseWord(library->lock_word, previous);
}

bool Font::AcquireFontLock(FontHandleNative* font, std::uint32_t& previous) {
    return font && AcquireMagicLock(font->lock_word, font->magic, HANDLE_MAGIC, previous);
}

void Font::ReleaseFontLock(FontHandleNative* font, std::uint32_t previous) {
    if (font) ReleaseWord(font->lock_word, previous);
}

bool Font::AcquireCachedStyleLock(FontHandleNative* font, std::uint32_t& previous) {
    return font && AcquireMagicLock(font->cached_style.cache_lock_word, font->magic, HANDLE_MAGIC, previous);
}

void Font::ReleaseCachedStyleLock(FontHandleNative* font, std::uint32_t previous) {
    if (font) ReleaseWord(font->cached_style.cache_lock_word, previous);
}

namespace {

FontHandle* AcquireFontList(Font::FontLibNative* library) {
    auto* const busy = reinterpret_cast<FontHandle*>(std::numeric_limits<std::uintptr_t>::max());
    std::atomic_ref<FontHandle*> slot(library->list_head_ptr);
    for (;;) {
        FontHandle* current = slot.load(std::memory_order_acquire);
        if (current != busy && slot.compare_exchange_weak(current, busy, std::memory_order_acq_rel)) return current;
        Font::Backoff();
    }
}

void ReleaseFontList(Font::FontLibNative* library, FontHandle* head) {
    std::atomic_ref<FontHandle*>(library->list_head_ptr).store(head, std::memory_order_release);
}

}

void Font::LinkFontToLibrary(FontLibNative* library, FontHandle handle) {
    FontHandle* head = AcquireFontList(library);
    if (head) {
        const FontHandle oldHead = *head;
        auto* font = GetNativeFont(handle);
        font->prevFont = nullptr;
        font->nextFont = oldHead;
        if (oldHead) GetNativeFont(oldHead)->prevFont = handle;
        *head = handle;
    }
    ReleaseFontList(library, head);
}

void Font::UnlinkFontFromLibrary(FontLibNative* library, FontHandle handle) {
    FontHandle* head = AcquireFontList(library);
    auto* font = GetNativeFont(handle);
    if (head) {
        if (font->prevFont) {
            GetNativeFont(font->prevFont)->nextFont = font->nextFont;
        } else if (*head == handle) {
            *head = font->nextFont;
        }
        if (font->nextFont) GetNativeFont(font->nextFont)->prevFont = font->prevFont;
    }
    font->prevFont = nullptr;
    font->nextFont = nullptr;
    ReleaseFontList(library, head);
}

std::uint32_t* Font::EntryLockWord(FontCtxEntry* entry, std::uint32_t modeLow) {
    if (modeLow == 3) return &entry->lock_mode3;
    if (modeLow == 1) return &entry->lock_mode1;
    return &entry->lock_mode2;
}

void** Font::EntryObjectSlot(FontCtxEntry* entry, std::uint32_t modeLow) {
    if (modeLow == 3) return &entry->obj_mode3;
    if (modeLow == 1) return &entry->obj_mode1;
    return &entry->obj_mode2;
}

Font::FontCtxEntry* Font::AcquireFontCtxEntry(void* ctx, std::uint32_t index, std::uint32_t modeLow, FontObj** outObject, std::uint32_t* outLockWord) {
    if (!ctx || !outObject || !outLockWord) return nullptr;
    auto* header = static_cast<FontCtxHeader*>(ctx);
    if (index >= header->max_entries || !header->base) {
        *outLockWord = 0;
        *outObject = nullptr;
        return nullptr;
    }
    auto* entry = static_cast<FontCtxEntry*>(header->base) + index;
    if (modeLow != 1 && modeLow != 2 && modeLow != 3) {
        *outLockWord = 0;
        *outObject = nullptr;
        return entry;
    }
    *outLockWord = AcquireWordLock(*EntryLockWord(entry, modeLow)) | LOCK_BIT;
    *outObject = static_cast<FontObj*>(*EntryObjectSlot(entry, modeLow));
    return entry;
}

void Font::ReleaseFontCtxEntryLock(FontCtxEntry* entry, std::uint32_t modeLow, std::uint32_t lockWord) {
    if (!entry) return;
    if ((lockWord & COUNT_MASK) == 0) *EntryObjectSlot(entry, modeLow) = nullptr;
    if ((lockWord & COUNT_MASK) == 0 && !entry->obj_mode1 && !entry->obj_mode3 && !entry->obj_mode2) {
        entry->unique_id = 0;
        entry->active = 0;
    }
    ReleaseWord(*EntryLockWord(entry, modeLow), lockWord);
}

Font::FontObj* Font::FindSubFont(FontObj* head, std::uint32_t subFontIndex) {
    for (auto* node = head; node; node = node->next) {
        if (node->sub_font_index == subFontIndex) return node;
    }
    return nullptr;
}

void* Font::FontContext(const FontLibNative* library, const FontHandleNative* font) {
    return font->open_info.fontset_flags != 0 ? library->sysfonts_ctx : library->external_fonts_ctx;
}

void Font::ReleaseFontObjectsForHandle(FontHandleNative* font) {
    auto* library = static_cast<FontLibNative*>(font->library);
    if (!library || library->magic != LIBRARY_MAGIC || !library->sys_driver || !library->sys_driver->close) return;
    const std::uint32_t fontId = font->open_info.ctx_entry_index;
    if (static_cast<std::int32_t>(fontId) < 0) return;
    const std::uint32_t modeLow = font->flags & 0x0Fu;
    FontObj* head = nullptr;
    std::uint32_t lockWord = 0;
    auto* entry = AcquireFontCtxEntry(FontContext(library, font), fontId, modeLow, &head, &lockWord);
    if (!entry) return;
    FontObj* match = FindSubFont(head, font->open_info.sub_font_index);
    const std::uint32_t openFlag = lockWord & OPEN_BIT;
    const std::uint32_t countBits = lockWord & COUNT_MASK;
    if (openFlag != 0 && countBits != 0 && match) {
        const std::uint32_t refcount = match->refcount;
        const std::uint32_t decremented = lockWord - 1;
        lockWord = decremented;
        if (refcount == 1) {
            FontObj* next = match->next;
            FontObj* prev = match->prev;
            library->sys_driver->close(match, 0);
            if (!prev) *EntryObjectSlot(entry, modeLow) = next;
        } else if (refcount != 0) {
            match->refcount = refcount - 1;
        }
        if (countBits == 1) lockWord = ~openFlag & decremented;
    }
    ReleaseFontCtxEntryLock(entry, modeLow, lockWord);
}

Font::FontState* Font::TryGetState(FontHandle handle) {
    if (!handle) return nullptr;
    std::lock_guard lock(stateMutex);
    const auto it = fontStates.find(handle);
    return it == fontStates.end() ? nullptr : it->second.get();
}

Font::FontState& Font::ResetState(FontHandle handle) {
    std::lock_guard lock(stateMutex);
    auto& slot = fontStates[handle];
    slot = std::make_unique<FontState>();
    return *slot;
}

void Font::RemoveState(FontHandle handle) {
    if (!handle) return;
    std::lock_guard lock(stateMutex);
    fontStates.erase(handle);
}

void Font::LoadStateFace(FontState& state, std::shared_ptr<const std::vector<unsigned char>> bytes, std::uint32_t subFontIndex) {
    if (state.face) {
        FT_Done_Face(state.face);
        state.face = nullptr;
    }
    state.faceData = std::move(bytes);
    const FT_Library library = HostFreeTypeLibrary();
    if (!library || !state.faceData || state.faceData->empty()) return;
    FT_Face face = nullptr;
    if (FT_New_Memory_Face(library, state.faceData->data(), static_cast<FT_Long>(state.faceData->size()), static_cast<FT_Long>(subFontIndex), &face) != 0) return;
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    state.face = face;
}

void Font::TrackGeneratedGlyph(FontGlyph glyph) {
    std::lock_guard lock(glyphMutex);
    generatedGlyphs.insert(glyph);
}

bool Font::ForgetGeneratedGlyph(FontGlyph glyph) {
    std::lock_guard lock(glyphMutex);
    return generatedGlyphs.erase(glyph) > 0;
}

Font::GeneratedGlyph* Font::TryGetGeneratedGlyph(FontGlyph glyph) {
    if (!glyph || glyph->magic != GLYPH_MAGIC) return nullptr;
    std::lock_guard lock(glyphMutex);
    if (!generatedGlyphs.contains(glyph)) return nullptr;
    return reinterpret_cast<GeneratedGlyph*>(glyph);
}

void Font::PopulateGlyphMetricVariants(GeneratedGlyph& glyph) {
    if (glyph.metricsInitialized) return;
    glyph.metricsHorizontal.width = glyph.metrics.width;
    glyph.metricsHorizontal.height = glyph.metrics.height;
    glyph.metricsHorizontal.horizontal.bearing_x = glyph.metrics.Horizontal.bearingX;
    glyph.metricsHorizontal.horizontal.bearing_y = glyph.metrics.Horizontal.bearingY;
    glyph.metricsHorizontal.horizontal.advance = glyph.metrics.Horizontal.advance;
    glyph.metricsHorizontalX.width = glyph.metrics.width;
    glyph.metricsHorizontalX.horizontal.bearing_x = glyph.metrics.Horizontal.bearingX;
    glyph.metricsHorizontalX.horizontal.advance = glyph.metrics.Horizontal.advance;
    glyph.metricsHorizontalAdvance.horizontal.advance = glyph.metrics.Horizontal.advance;
    glyph.metricsInitialized = true;
}

void Font::BuildBoundingOutline(GeneratedGlyph& glyph) {
    const float left = glyph.metrics.Horizontal.bearingX;
    const float top = glyph.metrics.Horizontal.bearingY;
    const float right = left + glyph.metrics.width;
    const float bottom = top - glyph.metrics.height;
    glyph.outlinePoints = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    glyph.outlineTags.assign(glyph.outlinePoints.size(), 1);
    glyph.outlineContours = {static_cast<std::uint16_t>(glyph.outlinePoints.size() - 1)};
    glyph.outline.points_ptr = glyph.outlinePoints.data();
    glyph.outline.tags_ptr = glyph.outlineTags.data();
    glyph.outline.contour_end_idx = glyph.outlineContours.data();
    glyph.outline.points_cnt = static_cast<std::int16_t>(glyph.outlinePoints.size());
    glyph.outline.contours_cnt = static_cast<std::int16_t>(glyph.outlineContours.size());
    glyph.outlineInitialized = true;
}

bool Font::ValidStyleFrame(const FontStyleFrame* frame) {
    return frame && frame->magic == STYLE_FRAME_MAGIC;
}

int Font::StyleStateSetScalePixel(StyleStateBlock* style, float w, float h) {
    if (!style) return 0;
    if (style->scale_w == w && style->scale_h == h && style->scale_unit == 0) return 0;
    style->scale_unit = 0;
    style->scale_w = w;
    style->scale_h = h;
    return 1;
}

int Font::StyleStateSetScalePoint(StyleStateBlock* style, float w, float h) {
    if (!style) return 0;
    if (style->scale_w == w && style->scale_h == h && style->scale_unit == 1) return 0;
    style->scale_unit = 1;
    style->scale_w = w;
    style->scale_h = h;
    return 1;
}

int Font::StyleStateGetScalePixel(const StyleStateBlock* style, float* w, float* h) {
    if (!style || (!w && !h)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool points = style->scale_unit != 0;
    if (w) {
        float value = style->scale_w;
        if (points && style->dpi_x != 0) value *= static_cast<float>(style->dpi_x) / POINTS_PER_INCH;
        *w = value;
    }
    if (h) {
        float value = style->scale_h;
        if (points && style->dpi_y != 0) value *= static_cast<float>(style->dpi_y) / POINTS_PER_INCH;
        *h = value;
    }
    return SCE_FONT_OK;
}

int Font::StyleStateGetScalePoint(const StyleStateBlock* style, float* w, float* h) {
    if (!style || (!w && !h)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool pixels = style->scale_unit == 0;
    if (w) {
        float value = style->scale_w;
        if (pixels && style->dpi_x != 0) value *= POINTS_PER_INCH / static_cast<float>(style->dpi_x);
        *w = value;
    }
    if (h) {
        float value = style->scale_h;
        if (pixels && style->dpi_y != 0) value *= POINTS_PER_INCH / static_cast<float>(style->dpi_y);
        *h = value;
    }
    return SCE_FONT_OK;
}

int Font::StyleStateSetDpi(StyleStateBlock* style, std::uint32_t hDpi, std::uint32_t vDpi) {
    if (!style) return 0;
    if (hDpi == 0) hDpi = 0x48;
    if (vDpi == 0) vDpi = 0x48;
    if (style->dpi_x == hDpi && style->dpi_y == vDpi) return 0;
    style->dpi_x = hDpi;
    style->dpi_y = vDpi;
    return 1;
}

int Font::StyleStateSetSlantRatio(StyleStateBlock* style, float slantRatio) {
    if (!style || style->slant_ratio == slantRatio) return 0;
    style->slant_ratio = std::clamp(slantRatio, -1.0f, 1.0f);
    return 1;
}

int Font::StyleStateGetSlantRatio(const StyleStateBlock* style, float* slantRatio) {
    if (!style || !slantRatio) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *slantRatio = style->slant_ratio;
    return SCE_FONT_OK;
}

int Font::StyleStateSetWeightScale(StyleStateBlock* style, float weightXScale, float weightYScale) {
    if (!style) return 0;
    const float deltaX = std::clamp(weightXScale - 1.0f, -0.04f, 0.04f);
    const float deltaY = std::clamp(weightYScale - 1.0f, -0.04f, 0.04f);
    bool changed = false;
    if (style->effect_weight_x != deltaX) {
        style->effect_weight_x = deltaX;
        changed = true;
    }
    if (style->effect_weight_y != deltaY) {
        style->effect_weight_y = deltaY;
        changed = true;
    }
    return changed ? 1 : 0;
}

int Font::StyleStateGetWeightScale(const StyleStateBlock* style, float* weightXScale, float* weightYScale, std::uint32_t* mode) {
    if (!weightXScale && !weightYScale && !mode) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!style) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (weightXScale) *weightXScale = style->effect_weight_x + 1.0f;
    if (weightYScale) *weightYScale = style->effect_weight_y + 1.0f;
    if (mode) *mode = 0;
    return SCE_FONT_OK;
}

std::uint8_t Font::CachedStyleCacheFlags(const CachedStyle& style) {
    return static_cast<std::uint8_t>(style.cache_flags_and_direction & 0xFFu);
}

void Font::CachedStyleSetCacheFlags(CachedStyle& style, std::uint8_t flags) {
    style.cache_flags_and_direction = (style.cache_flags_and_direction & 0xFFFFFF00u) | flags;
}

void Font::CachedStyleSetDirectionWord(CachedStyle& style, std::uint16_t word) {
    style.cache_flags_and_direction = (style.cache_flags_and_direction & 0x0000FFFFu) | (static_cast<std::uint32_t>(word) << 16);
}

float Font::CachedStyleGetScalar(const CachedStyle& style) {
    float value;
    std::memcpy(&value, &style.cached_scalar_bits, sizeof(value));
    return value;
}

void Font::CachedStyleSetScalar(CachedStyle& style, float value) {
    std::memcpy(&style.cached_scalar_bits, &value, sizeof(value));
}

void Font::ClearRenderOutputs(FontGlyphMetrics* metrics, FontRenderOutput* result) {
    if (metrics) *metrics = {};
    if (result) *result = {};
}
