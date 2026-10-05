// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTDRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTDRIVER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "prx/libSceFont/include/FontTypes.hpp"

namespace Font {

constexpr std::uint16_t MEMORY_MAGIC = 0x0F00;
constexpr std::uint16_t LIBRARY_MAGIC = 0x0F01;
constexpr std::uint16_t HANDLE_MAGIC = 0x0F02;
constexpr std::uint16_t GLYPH_MAGIC = 0x0F03;
constexpr std::uint16_t RENDERER_MAGIC = 0x0F07;
constexpr std::uint16_t STYLE_FRAME_MAGIC = 0x0F09;

constexpr std::uint32_t LOCK_BIT = 0x80000000u;
constexpr std::uint32_t OPEN_BIT = 0x40000000u;
constexpr std::uint32_t COUNT_MASK = 0x0FFFFFFFu;
constexpr std::uint64_t SYSFONT_FLAG_SYSTEM_SET = 0x100u;

constexpr std::size_t HORIZONTAL_LAYOUT_SIZE = 0x30;
constexpr std::size_t HORIZONTAL_LINE_ADVANCE = 0x00;
constexpr std::size_t HORIZONTAL_BASELINE = 0x04;
constexpr std::size_t HORIZONTAL_X_BOUND_LO = 0x08;
constexpr std::size_t HORIZONTAL_X_BOUND_HI = 0x0C;
constexpr std::size_t HORIZONTAL_MAX_ADVANCE_WIDTH = 0x10;
constexpr std::size_t HORIZONTAL_CARET_RISE_ADJUST = 0x14;
constexpr std::size_t HORIZONTAL_EFFECT_HEIGHT = 0x18;
constexpr std::size_t HORIZONTAL_HALF_EFFECT_WIDTH = 0x1C;
constexpr std::size_t HORIZONTAL_LEFT_ADJUST = 0x20;

constexpr std::size_t VERTICAL_LAYOUT_SIZE = 0x20;
constexpr std::size_t VERTICAL_COLUMN_ADVANCE = 0x00;
constexpr std::size_t VERTICAL_BASELINE_OFFSET_X = 0x04;
constexpr std::size_t VERTICAL_METRICS_0X08 = 0x08;
constexpr std::size_t VERTICAL_METRICS_0X0C = 0x0C;
constexpr std::size_t VERTICAL_ADVANCE_HEIGHT = 0x10;
constexpr std::size_t VERTICAL_DECORATION_SPAN = 0x14;
constexpr std::size_t VERTICAL_DECORATION_0X08 = 0x18;
constexpr std::size_t VERTICAL_DECORATION_0X0C = 0x1C;

inline float LoadFloat(const std::uint8_t* base, std::size_t offset) {
    float value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

inline void StoreFloat(std::uint8_t* base, std::size_t offset, float value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

struct StyleStateBlock {
    std::uint32_t dpi_x;
    std::uint32_t dpi_y;
    std::uint32_t scale_unit;
    std::uint32_t reserved_0x0c;
    float scale_w;
    float scale_h;
    float effect_weight_x;
    float effect_weight_y;
    float slant_ratio;
    float reserved_0x24;
};

struct CachedStyle {
    StyleStateBlock state;
    std::uint32_t reserved_0x28;
    std::uint32_t layout_cache_state;
    std::uint32_t cache_flags_and_direction;
    std::uint32_t cache_lock_word;
    std::uint8_t layout_cache_bytes[0x20];
    std::uint32_t reserved_0x58;
    std::uint32_t cached_scalar_bits;
};

struct FontHandleOpenInfo {
    std::uint32_t unique_id_packed;
    std::uint32_t ctx_entry_index;
    std::uint32_t sub_font_index;
    std::uint32_t fontset_flags;
    const void* fontset_record;
    std::uint64_t reserved_0x18;
};

struct FontHandleNative {
    std::uint16_t magic;
    std::uint16_t flags;
    std::uint32_t lock_word;
    FontHandleOpenInfo open_info;
    FontLibrary library;
    void* renderer;
    std::uint8_t reserved_0x38[0x08];
    StyleStateBlock style;
    CachedStyle cached_style;
    std::uint16_t metricA;
    std::uint16_t metricB;
    std::uint8_t reserved_0xcc[0x1C];
    std::uint8_t reserved_0xe8[0x08];
    FontHandle prevFont;
    FontHandle nextFont;
};

struct FontObj {
    std::uint32_t refcount;
    std::uint32_t reserved_0x04;
    std::uint32_t reserved_0x08;
    std::uint32_t sub_font_index;
    FontObj* prev;
    FontObj* next;
    std::uint8_t reserved_0x20[0x08];
    void* open_ctx_0x28;
    void* ft_face;
    FontHandle font_handle;
    std::int32_t shift_units_x;
    std::int32_t shift_units_y;
    std::uint64_t layout_seed_pair;
    float scale_x_0x50;
    float scale_y_0x54;
    void* ft_ctx_0x58;
    std::uint8_t reserved_0x60[0x04];
    std::int32_t cached_glyph_index_0x64;
    std::uint64_t cached_units_x_0x68;
    std::uint64_t cached_units_y_0x70;
    std::int64_t shift_cache_x;
    std::int64_t shift_cache_y;
    std::array<std::uint64_t, 2> layout_seed_vec;
    std::array<std::uint64_t, 2> layout_scale_vec;
    std::uint8_t reserved_0xA8[0x130 - 0xA8];
    std::uint32_t glyph_cfg_word_0x130;
    std::uint8_t glyph_cfg_mode_0x134;
    std::uint8_t glyph_cfg_byte_0x135;
    std::uint8_t glyph_cfg_byte_0x136;
    std::uint8_t reserved_0x137[0x200 - 0x137];
};

struct FontCtxHeader {
    std::uint32_t lock_word;
    std::uint32_t max_entries;
    void* base;
    std::uint8_t reserved_0x10[0x10];
};

struct FontCtxEntry {
    std::uint32_t reserved_0x00;
    std::uint32_t active;
    std::uint64_t font_address;
    std::uint32_t unique_id;
    std::uint32_t lock_mode1;
    std::uint32_t lock_mode3;
    std::uint32_t lock_mode2;
    void* obj_mode1;
    void* obj_mode3;
    void* obj_mode2;
    std::uint8_t reserved_0x38[0x08];
};

struct SysDriver;

struct FontLibNative {
    std::uint16_t magic;
    std::uint16_t reserved0;
    std::uint32_t lock_word;
    std::uint32_t flags;
    std::uint32_t feature_word;
    std::uint32_t mem_kind;
    std::uint32_t region_size;
    void* region_base;
    void* alloc_ctx;
    const FontMemoryInterface* iface;
    std::uint8_t reserved_0x30[0x20];
    FontAllocFunction alloc_fn;
    FontFreeFunction dealloc_fn;
    FontReallocFunction realloc_fn;
    FontCallocFunction calloc_fn;
    std::uint8_t reserved_0x70[0x10];
    const SysDriver* sys_driver;
    void* fontset_registry;
    std::uint8_t reserved_0x90[0x10];
    void* sysfonts_ctx;
    void* external_fonts_ctx;
    std::uint32_t* device_cache_buf;
    std::uint8_t reserved_0xb8[0x04];
    std::uint32_t workspace_size;
    void* workspace;
    void* sysfont_desc_ptr;
    std::uint64_t sysfont_flags;
    FontHandle* list_head_ptr;
    FontHandle list_head;
    std::uint8_t reserved_0xe8[0x18];
};

using DriverPixelResolutionFunction = std::uint32_t (APS5_VABI*)();
using DriverInitFunction = int (APS5_VABI*)(const FontMemory* memory, FontLibNative* library);
using DriverTermFunction = int (APS5_VABI*)(FontLibNative* library);
using DriverSupportFunction = int (APS5_VABI*)(FontLibNative* library, std::uint32_t formats);
using DriverOpenFunction = int (APS5_VABI*)(FontLibNative* library, std::uint32_t mode, const void* fontAddress, std::uint32_t fontSize, std::uint32_t subFontIndex, std::uint32_t uniqueWord, FontObj** inoutFontObj);
using DriverCloseFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t flags);
using DriverScaleFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint16_t* outUnitsPerEm, float* outScale);
using DriverMetricFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t metricId, std::uint16_t* outMetric);
using DriverGlyphsCountFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t* outCount);
using DriverGlyphIndexFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t codepoint, std::uint32_t* outGlyphIndex);
using DriverSetCharWithDpiFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t dpiX, std::uint32_t dpiY, float scaleX, float scaleY, float* outScaleX, float* outScaleY);
using DriverSetCharDefaultDpiFunction = int (APS5_VABI*)(FontObj* fontObj, float scaleX, float scaleY, float* outScaleX, float* outScaleY);
using DriverComputeLayoutFunction = int (APS5_VABI*)(FontObj* fontObj, const StyleStateBlock* style, std::uint8_t* outWords);
using DriverLoadGlyphCachedFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t glyphIndex, std::int32_t mode, std::uint64_t* outWords);
using DriverGetGlyphMetricsFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t* optParam2, std::uint8_t mode, std::uint8_t* outParams, FontGlyphMetrics* outMetrics);
using DriverApplyGlyphAdjustFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t p2, std::uint32_t glyphIndex, std::int32_t p4, std::int32_t p5, std::uint32_t* inoutGlyphIndex);
using DriverConfigureGlyphFunction = int (APS5_VABI*)(FontObj* fontObj, std::uint32_t* inParams, std::int32_t mode, std::uint32_t* inoutState);

struct SysDriver {
    std::uint32_t magic;
    std::uint32_t reserved;
    void* reserved_ptr1;
    DriverPixelResolutionFunction pixel_resolution;
    DriverInitFunction init;
    DriverTermFunction term;
    DriverSupportFunction support_formats;
    std::uint8_t reserved_0x30[0x08];
    DriverOpenFunction open;
    DriverCloseFunction close;
    std::uint8_t reserved_0x48[0x08];
    DriverScaleFunction scale;
    std::uint8_t reserved_0x58[0x08];
    DriverMetricFunction metric;
    DriverGlyphsCountFunction glyphs_count;
    std::uint8_t reserved_0x70[0x08];
    DriverGlyphIndexFunction glyph_index;
    DriverSetCharWithDpiFunction set_char_with_dpi;
    DriverSetCharDefaultDpiFunction set_char_default_dpi;
    std::uint8_t reserved_0x90[0x10];
    DriverComputeLayoutFunction compute_layout;
    DriverLoadGlyphCachedFunction load_glyph_cached;
    std::uint8_t reserved_0xb0[0x08];
    DriverGetGlyphMetricsFunction get_glyph_metrics;
    std::uint8_t reserved_0xc0[0x20];
    DriverApplyGlyphAdjustFunction apply_glyph_adjust;
    std::uint8_t reserved_0xe8[0x20];
    DriverConfigureGlyphFunction configure_glyph;
    std::uint8_t reserved_0x110[0x08];
    DriverComputeLayoutFunction compute_layout_alt;
};

struct RendererNative {
    std::uint16_t magic;
    std::uint16_t reserved02;
    std::uint8_t reserved_04[0x08];
    std::uint8_t feature_byte_0x0c;
    std::uint8_t reserved_0d[0x03];
    std::uint32_t mem_kind;
    std::uint32_t region_size;
    void* region_base;
    void* alloc_ctx;
    const FontMemoryInterface* mem_iface;
    std::uint8_t reserved_30[0x20];
    FontAllocFunction alloc_fn;
    FontFreeFunction free_fn;
    FontReallocFunction realloc_fn;
    FontCallocFunction calloc_fn;
    std::uint8_t reserved_70[0x10];
    void* selection;
    std::uint8_t reserved_88[0x08];
    std::uint64_t outline_magic_0x90;
    void* workspace;
    std::uint64_t workspace_size;
    std::uint32_t reserved_a8;
    std::uint32_t outline_policy_flag;
};

struct RendererFtBackend {
    void* renderer_header_0x10;
    std::uintptr_t unknown_0x08;
    std::uintptr_t unknown_0x10;
    std::uintptr_t unknown_0x18;
    void* initialized_marker;
    std::uint8_t reserved_0x28[0x20];
};

struct RendererFt {
    RendererNative base;
    std::uint8_t reserved_0x0B0[0x70];
    RendererFtBackend ft_backend;
};

using RendererCreateFunction = int (APS5_VABI*)(RendererNative* renderer);
using RendererDestroyFunction = int (APS5_VABI*)(RendererNative* renderer);
using RendererQueryFunction = std::uint64_t (APS5_VABI*)(RendererNative* renderer, std::uint8_t* params, std::int64_t* outPtr, std::uint8_t* outVector);

struct RendererSelection {
    std::uint32_t magic;
    std::uint32_t size;
    RendererCreateFunction create_fn;
    RendererDestroyFunction destroy_fn;
    RendererQueryFunction query_fn;
};

static_assert(sizeof(StyleStateBlock) == 0x28);
static_assert(offsetof(StyleStateBlock, scale_w) == 0x10);
static_assert(offsetof(StyleStateBlock, slant_ratio) == 0x20);
static_assert(sizeof(CachedStyle) == 0x60);
static_assert(offsetof(CachedStyle, layout_cache_state) == 0x2C);
static_assert(offsetof(CachedStyle, cache_lock_word) == 0x34);
static_assert(offsetof(CachedStyle, layout_cache_bytes) == 0x38);
static_assert(offsetof(CachedStyle, cached_scalar_bits) == 0x5C);
static_assert(sizeof(FontHandleOpenInfo) == 0x20);
static_assert(sizeof(FontHandleNative) == 0x100);
static_assert(offsetof(FontHandleNative, open_info) == 0x08);
static_assert(offsetof(FontHandleNative, library) == 0x28);
static_assert(offsetof(FontHandleNative, renderer) == 0x30);
static_assert(offsetof(FontHandleNative, style) == 0x40);
static_assert(offsetof(FontHandleNative, cached_style) == 0x68);
static_assert(offsetof(FontHandleNative, metricA) == 0xC8);
static_assert(offsetof(FontHandleNative, prevFont) == 0xF0);
static_assert(sizeof(FontObj) == 0x200);
static_assert(offsetof(FontObj, ft_face) == 0x30);
static_assert(offsetof(FontObj, shift_cache_x) == 0x78);
static_assert(offsetof(FontObj, layout_scale_vec) == 0x98);
static_assert(offsetof(FontObj, glyph_cfg_word_0x130) == 0x130);
static_assert(sizeof(FontCtxHeader) == 0x20);
static_assert(sizeof(FontCtxEntry) == 0x40);
static_assert(offsetof(FontCtxEntry, obj_mode2) == 0x30);
static_assert(sizeof(FontLibNative) == 0x100);
static_assert(offsetof(FontLibNative, alloc_ctx) == 0x20);
static_assert(offsetof(FontLibNative, iface) == 0x28);
static_assert(offsetof(FontLibNative, alloc_fn) == 0x50);
static_assert(offsetof(FontLibNative, sys_driver) == 0x80);
static_assert(offsetof(FontLibNative, fontset_registry) == 0x88);
static_assert(offsetof(FontLibNative, sysfonts_ctx) == 0xA0);
static_assert(offsetof(FontLibNative, external_fonts_ctx) == 0xA8);
static_assert(offsetof(FontLibNative, device_cache_buf) == 0xB0);
static_assert(offsetof(FontLibNative, workspace) == 0xC0);
static_assert(offsetof(FontLibNative, sysfont_flags) == 0xD0);
static_assert(offsetof(FontLibNative, list_head_ptr) == 0xD8);
static_assert(offsetof(FontLibNative, list_head) == 0xE0);
static_assert(sizeof(SysDriver) == 0x120);
static_assert(offsetof(SysDriver, pixel_resolution) == 0x10);
static_assert(offsetof(SysDriver, open) == 0x38);
static_assert(offsetof(SysDriver, scale) == 0x50);
static_assert(offsetof(SysDriver, glyph_index) == 0x78);
static_assert(offsetof(SysDriver, compute_layout) == 0xA0);
static_assert(offsetof(SysDriver, get_glyph_metrics) == 0xB8);
static_assert(offsetof(SysDriver, apply_glyph_adjust) == 0xE0);
static_assert(offsetof(SysDriver, configure_glyph) == 0x108);
static_assert(offsetof(SysDriver, compute_layout_alt) == 0x118);
static_assert(sizeof(RendererNative) == 0xB0);
static_assert(offsetof(RendererNative, alloc_fn) == 0x50);
static_assert(offsetof(RendererNative, selection) == 0x80);
static_assert(offsetof(RendererNative, outline_policy_flag) == 0xAC);
static_assert(sizeof(RendererFtBackend) == 0x48);
static_assert(sizeof(RendererFt) == 0x168);
static_assert(offsetof(RendererFt, ft_backend) == 0x120);

}

#endif
