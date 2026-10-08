// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTTYPES_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTTYPES_HPP

#include <cstddef>
#include <cstdint>

#include "prx/libc/include/General.hpp"

constexpr int SCE_FONT_OK = 0;
constexpr int SCE_FONT_ERROR_FATAL = static_cast<int>(0x80460001);
constexpr int SCE_FONT_ERROR_INVALID_PARAMETER = static_cast<int>(0x80460002);
constexpr int SCE_FONT_ERROR_INVALID_MEMORY = static_cast<int>(0x80460003);
constexpr int SCE_FONT_ERROR_INVALID_LIBRARY = static_cast<int>(0x80460004);
constexpr int SCE_FONT_ERROR_INVALID_FONT_HANDLE = static_cast<int>(0x80460005);
constexpr int SCE_FONT_ERROR_INVALID_GLYPH = static_cast<int>(0x80460006);
constexpr int SCE_FONT_ERROR_INVALID_RENDERER = static_cast<int>(0x80460007);
constexpr int SCE_FONT_ERROR_INVALID_TEXT_SOURCE = static_cast<int>(0x80460008);
constexpr int SCE_FONT_ERROR_INVALID_STRING = static_cast<int>(0x80460009);
constexpr int SCE_FONT_ERROR_INVALID_WRITING = static_cast<int>(0x8046000A);
constexpr int SCE_FONT_ERROR_INVALID_WORDS = static_cast<int>(0x8046000B);
constexpr int SCE_FONT_ERROR_ALLOCATION_FAILED = static_cast<int>(0x80460010);
constexpr int SCE_FONT_ERROR_FS_OPEN_FAILED = static_cast<int>(0x80460011);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_LIBRARY = static_cast<int>(0x80460018);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_FORMAT = static_cast<int>(0x80460019);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_FUNCTION = static_cast<int>(0x80460020);
constexpr int SCE_FONT_ERROR_ALREADY_SPECIFIED = static_cast<int>(0x80460021);
constexpr int SCE_FONT_ERROR_ALREADY_ATTACHED = static_cast<int>(0x80460022);
constexpr int SCE_FONT_ERROR_ALREADY_OPENED = static_cast<int>(0x80460023);
constexpr int SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER = static_cast<int>(0x80460025);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_FONTSET = static_cast<int>(0x80460031);
constexpr int SCE_FONT_ERROR_FONT_OPEN_MAX = static_cast<int>(0x80460033);
constexpr int SCE_FONT_ERROR_FONT_OPEN_FAILED = static_cast<int>(0x80460036);
constexpr int SCE_FONT_ERROR_FONT_CLOSE_FAILED = static_cast<int>(0x80460037);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY = static_cast<int>(0x80460040);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_CODE = static_cast<int>(0x80460041);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_GLYPH = static_cast<int>(0x80460042);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_SCRIPT = static_cast<int>(0x80460043);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_LANGUAGE = static_cast<int>(0x80460044);
constexpr int SCE_FONT_ERROR_NO_SUPPORT_SURFACE = static_cast<int>(0x80460050);
constexpr int SCE_FONT_ERROR_UNSET_PARAMETER = static_cast<int>(0x80460058);
constexpr int SCE_FONT_ERROR_FUNCTIONAL_LIMIT = static_cast<int>(0x8046005C);
constexpr int SCE_FONT_ERROR_ALREADY_BOUND_RENDERER = static_cast<int>(0x80460060);
constexpr int SCE_FONT_ERROR_NOT_BOUND_RENDERER = static_cast<int>(0x80460061);
constexpr int SCE_FONT_ERROR_RENDERER_ALLOCATION_FAILED = static_cast<int>(0x80460063);
constexpr int SCE_FONT_ERROR_RENDERER_ALLOCATION_LIMITED = static_cast<int>(0x80460064);
constexpr int SCE_FONT_ERROR_RENDERER_RENDER_FAILED = static_cast<int>(0x80460065);

struct FontHandleOpaque {
    std::uint32_t reserved[64];
};

using FontHandle = FontHandleOpaque*;
using FontLibrary = void*;
using FontRenderer = void*;

struct FontMemory;

using FontAllocFunction = void* (APS5_VABI*)(void* object, std::uint32_t size);
using FontFreeFunction = void (APS5_VABI*)(void* object, void* p);
using FontReallocFunction = void* (APS5_VABI*)(void* object, void* p, std::uint32_t newSize);
using FontCallocFunction = void* (APS5_VABI*)(void* object, std::uint32_t nBlock, std::uint32_t size);
using FontMspaceCreateFunction = void* (APS5_VABI*)(void* parent, const char* name, void* address, std::uint32_t size, std::uint32_t attr);
using FontMspaceDestroyFunction = void (APS5_VABI*)(void* parent, void* mspace);
using FontMemoryDestroyFunction = void (APS5_VABI*)(FontMemory* fontMemory, void* object, void* destroyArg);

struct FontMemoryInterface {
    FontAllocFunction alloc;
    FontFreeFunction dealloc;
    FontReallocFunction realloc_fn;
    FontCallocFunction calloc_fn;
    FontMspaceCreateFunction mspace_create;
    FontMspaceDestroyFunction mspace_destroy;
};

struct FontMemory {
    std::uint16_t mem_kind;
    std::uint16_t attr_bits;
    std::uint32_t region_size;
    void* region_base;
    void* mspace_handle;
    const FontMemoryInterface* iface;
    FontMemoryDestroyFunction on_destroy;
    void* destroy_ctx;
    void* some_ctx1;
    void* some_ctx2;
};

struct FontOpenDetail {
    std::uint16_t tag;
    std::uint16_t pad16;
    std::uint32_t flags;
    std::uint32_t subfont_index;
    std::int32_t unique_id;
    const void* reserved_ptr2;
    const void* reserved_ptr1;
};

struct FontGlyphMetrics {
    float width;
    float height;
    struct {
        float bearingX;
        float bearingY;
        float advance;
    } Horizontal;
    struct {
        float bearingX;
        float bearingY;
        float advance;
    } Vertical;
};

struct FontGlyphMetricsHorizontal {
    float width;
    float height;
    struct {
        float bearing_x;
        float bearing_y;
        float advance;
    } horizontal;
};

struct FontGlyphMetricsHorizontalX {
    float width;
    struct {
        float bearing_x;
        float advance;
    } horizontal;
};

struct FontGlyphMetricsHorizontalAdvance {
    struct {
        float advance;
    } horizontal;
};

struct FontKerning {
    float offsetX;
    float offsetY;
    float positionX;
    float positionY;
};

struct FontGlyphImageMetrics {
    float bearingX;
    float bearingY;
    float advance;
    float stride;
    std::uint32_t width;
    std::uint32_t height;
};

struct FontGenerateGlyphDetail {
    std::uint16_t id;
    std::uint16_t res0;
    std::uint16_t form_options;
    std::uint8_t glyph_form;
    std::uint8_t metrics_form;
    const FontMemory* mem;
    void* res1;
    void* res2;
};

struct FontGlyphOutlinePoint {
    float x;
    float y;
};

struct FontGlyphOutline {
    std::int16_t contours_cnt;
    std::int16_t points_cnt;
    std::uint32_t outline_flags;
    FontGlyphOutlinePoint* points_ptr;
    std::uint8_t* tags_ptr;
    std::uint16_t* contour_end_idx;
};

struct FontGlyphOpaque {
    std::uint16_t magic;
    std::uint16_t flags;
    std::uint8_t glyph_form;
    std::uint8_t metrics_form;
    std::uint16_t em_size;
    std::uint16_t baseline;
    std::uint16_t height_px;
    std::uint16_t origin_x;
    std::uint16_t origin_y;
    float scale_x;
    float base_scale;
    const FontMemory* memory;
};

using FontGlyph = FontGlyphOpaque*;

struct FontResultStage {
    std::uint8_t* p_00;
    std::uint32_t u32_08;
    std::uint32_t u32_0C;
    std::uint32_t u32_10;
};

struct FontSurfaceImage {
    std::uint8_t* address;
    std::uint32_t widthByte;
    std::uint8_t pixelSizeByte;
    std::uint8_t pixelFormat;
    std::uint16_t pad16;
};

struct FontRenderOutput {
    const FontResultStage* stage;
    FontSurfaceImage SurfaceImage;
    struct {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t w;
        std::uint32_t h;
    } UpdateRect;
    FontGlyphImageMetrics ImageMetrics;
};

struct FontTextCharacter {
    FontTextCharacter* prev;
    FontTextCharacter* next;
    void* textOrder;
    FontHandle font;
    void* shapeEntry;
    std::uint32_t characterCode;
    std::uint8_t reserved_0x2c[4];
    std::uint8_t clusterSpan;
    std::uint8_t clusterIndex;
    std::uint8_t clusterKind;
    std::int8_t synthetic;
    std::uint8_t reserved_0x34[4];
    std::uint64_t flags;
    std::uint8_t reserved_0x40[136];
};

struct FontTextCodes {
    void* order;
    std::uint32_t code;
    std::uint32_t reserved;
    FontTextCharacter* current;
    FontTextCharacter* term;
    void* systemReserved[4];
};

struct FontRenderSurface {
    void* buffer;
    std::int32_t widthByte;
    std::int8_t pixelSizeByte;
    std::uint8_t pad0;
    std::uint8_t styleFlag;
    std::uint8_t pad2;
    std::int32_t width;
    std::int32_t height;
    std::uint32_t sc_x0;
    std::uint32_t sc_y0;
    std::uint32_t sc_x1;
    std::uint32_t sc_y1;
    std::uint64_t reserved_q[11];
};

struct FontStyleFrame {
    std::uint16_t magic;
    std::uint8_t flags1;
    std::uint8_t flags2;
    std::uint32_t hDpi;
    std::uint32_t vDpi;
    std::uint32_t scaleUnit;
    float baseScale;
    float scalePixelW;
    float scalePixelH;
    float effectWeightX;
    float effectWeightY;
    float slantRatio;
    std::uint32_t reserved_0x28;
    std::uint32_t layout_cache_state;
    std::uint32_t cache_flags_and_direction;
    std::uint32_t cache_lock_word;
    std::uint8_t layout_cache_bytes[0x20];
    std::uint32_t reserved_0x58;
    std::uint32_t cached_scalar_bits;
};

struct FontTextSource;

union FontTextParseResult {
    void* reserved[4];
    struct {
        FontHandle font;
        std::uint32_t code;
    } FontCode;
    struct {
        void* reserved;
        std::uint32_t terminateCode;
    } Terminate;
    struct {
        void* reserved;
        std::int32_t errorCode;
    } Error;
};

using FontTextParseFunction = std::int32_t (APS5_VABI*)(FontTextSource* fontTextSource, void** order, FontTextParseResult* result);

struct FontTextSource {
    std::uint64_t systemUse0;
    const void* start;
    const void* end;
    const void* current;
    FontTextParseFunction textParser;
    void* textObject;
    FontHandle defaultFont;
    void* systemUse[5];
};

struct FontCreateStringDetail {
    std::uint16_t detailId;
    std::uint8_t detailType;
    std::uint8_t detections;
    std::uint32_t ordersOption;
    FontHandle defaultFont;
    union {
        struct {
            void* function;
            void* object;
        } Callback;
    } Orders;
};

struct FontStringOpaque;
using FontString = FontStringOpaque*;

struct FontWriting {
    void* systemUse[32];
};

struct FontWritingProfile {
    std::uint32_t characterCount : 8;
    std::uint32_t invisibleGlyph : 1;
    std::uint32_t reserved : 23;
};

struct FontWritingStep {
    float x;
    float y;
    float advanceX;
    float advanceY;
    FontHandle font;
    FontWritingProfile Profile;
    std::uint32_t glyphCode;
    struct {
        float x;
        float y;
    } Positioning;
    FontGlyphMetrics GlyphMetrics;
};

struct FontWritingLetterStep {
    float x;
    float y;
    float advanceX;
    float advanceY;
    struct {
        std::uint32_t textsCount;
        std::uint32_t textsIndex;
        std::uint32_t glyphsCount;
        std::uint32_t glyphsIndex;
        std::uint8_t baseComponentsCount;
        std::uint8_t baseComponentsIndex;
        std::uint8_t baseFocusCount;
        std::uint8_t baseFocusIndex;
        std::uint8_t oppositeDirection;
        std::uint8_t characterTextCount;
        std::uint8_t baseTextCount;
        std::uint8_t markTextCount;
        std::uint32_t marksTextCount;
        std::uint32_t marksTextNumber;
        std::uint32_t reserved[3];
        std::int32_t textLetterOffset;
    } Components;
};

struct FontWritingExtent {
    float top;
    float bottom;
    float left;
    float right;
};

struct FontWritingMetrics {
    float advanceX;
    float advanceY;
    FontWritingExtent Extent;
};

struct FontWritingLineStep {
    float x;
    float y;
    float advanceX;
    float advanceY;
    float spacingProgress;
    void* writingOrderer;
    struct {
        float x;
        float y;
    } Adjusting;
    FontWritingMetrics Metrics;
};

struct FontHorizontalLayout {
    float baselineOffset;
    float lineAdvance;
    float decorationExtent;
};

struct FontVerticalLayout {
    float baselineOffsetX;
    float columnAdvance;
    float decorationSpan;
};

static_assert(sizeof(FontHandleOpaque) == 0x100);
static_assert(sizeof(FontMemory) == 0x40);
static_assert(sizeof(FontMemoryInterface) == 0x30);
static_assert(sizeof(FontOpenDetail) == 0x20);
static_assert(sizeof(FontGlyphMetrics) == 0x20);
static_assert(sizeof(FontKerning) == 0x10);
static_assert(sizeof(FontGlyphImageMetrics) == 0x18);
static_assert(sizeof(FontGenerateGlyphDetail) == 0x20);
static_assert(sizeof(FontResultStage) == 0x18);
static_assert(sizeof(FontSurfaceImage) == 0x10);
static_assert(sizeof(FontRenderOutput) == 0x40);
static_assert(sizeof(FontTextCharacter) == 0xC8);
static_assert(offsetof(FontTextCharacter, flags) == 0x38);
static_assert(sizeof(FontTextCodes) == 0x40);
static_assert(offsetof(FontTextCodes, current) == 0x10);
static_assert(offsetof(FontTextCodes, term) == 0x18);
static_assert(sizeof(FontRenderSurface) == 0x80);
static_assert(sizeof(FontStyleFrame) == 0x60);
static_assert(offsetof(FontStyleFrame, scalePixelW) == 0x14);
static_assert(offsetof(FontStyleFrame, cache_lock_word) == 0x34);
static_assert(offsetof(FontStyleFrame, reserved_0x58) == 0x58);
static_assert(sizeof(FontTextSource) == 0x60);
static_assert(offsetof(FontTextSource, textParser) == 0x20);
static_assert(offsetof(FontTextSource, defaultFont) == 0x30);
static_assert(offsetof(FontTextSource, systemUse) == 0x38);
static_assert(sizeof(FontCreateStringDetail) == 0x20);
static_assert(sizeof(FontWriting) == 0x100);
static_assert(sizeof(FontWritingProfile) == sizeof(std::uint32_t));
static_assert(sizeof(FontWritingStep) == 0x48);
static_assert(sizeof(FontWritingLetterStep) == 0x40);
static_assert(sizeof(FontWritingMetrics) == 0x18);
static_assert(sizeof(FontWritingLineStep) == 0x40);
static_assert(offsetof(FontWritingLineStep, writingOrderer) == 0x18);

#endif
