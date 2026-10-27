#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTFREETYPE_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTFREETYPE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestLibrary;

struct GuestGeneric {
    void* data;
    void (APS5_VABI *finalizer)(void*);
};

struct GuestVector {
    std::int64_t x;
    std::int64_t y;
};

struct GuestMatrix {
    std::int64_t xx;
    std::int64_t xy;
    std::int64_t yx;
    std::int64_t yy;
};

struct GuestBBox {
    std::int64_t xMin;
    std::int64_t yMin;
    std::int64_t xMax;
    std::int64_t yMax;
};

struct GuestBitmapSize {
    std::int16_t height;
    std::int16_t width;
    std::int64_t size;
    std::int64_t x_ppem;
    std::int64_t y_ppem;
};

struct GuestCharMapRec {
    void* face;
    std::int32_t encoding;
    std::uint16_t platform_id;
    std::uint16_t encoding_id;
};

struct GuestSfntName {
    std::uint16_t platform_id;
    std::uint16_t encoding_id;
    std::uint16_t language_id;
    std::uint16_t name_id;
    unsigned char* string;
    std::uint32_t string_len;
};

struct GuestSizeMetrics {
    std::uint16_t x_ppem;
    std::uint16_t y_ppem;
    std::int64_t x_scale;
    std::int64_t y_scale;
    std::int64_t ascender;
    std::int64_t descender;
    std::int64_t height;
    std::int64_t max_advance;
};

struct GuestSizeRec {
    void* face;
    GuestGeneric generic;
    GuestSizeMetrics metrics;
    void* internal;
};

struct GuestSizeRequestRec {
    std::uint32_t type;
    std::int64_t width;
    std::int64_t height;
    std::uint32_t horiResolution;
    std::uint32_t vertResolution;
};

struct GuestGlyphMetrics {
    std::int64_t width;
    std::int64_t height;
    std::int64_t horiBearingX;
    std::int64_t horiBearingY;
    std::int64_t horiAdvance;
    std::int64_t vertBearingX;
    std::int64_t vertBearingY;
    std::int64_t vertAdvance;
};

struct GuestBitmap {
    std::uint32_t rows;
    std::uint32_t width;
    std::int32_t pitch;
    unsigned char* buffer;
    std::uint16_t num_grays;
    unsigned char pixel_mode;
    unsigned char palette_mode;
    void* palette;
};

struct GuestOutline {
    std::uint16_t n_contours;
    std::uint16_t n_points;
    GuestVector* points;
    unsigned char* tags;
    std::uint16_t* contours;
    std::int32_t flags;
};

struct GuestOutlineFuncs {
    int (APS5_VABI *move_to)(const GuestVector*, void*);
    int (APS5_VABI *line_to)(const GuestVector*, void*);
    int (APS5_VABI *conic_to)(const GuestVector*, const GuestVector*, void*);
    int (APS5_VABI *cubic_to)(const GuestVector*, const GuestVector*, const GuestVector*, void*);
    std::int32_t shift;
    std::int64_t delta;
};

struct GuestGlyphSlotRec {
    void* library;
    void* face;
    void* next;
    std::uint32_t glyph_index;
    GuestGeneric generic;
    GuestGlyphMetrics metrics;
    std::int64_t linearHoriAdvance;
    std::int64_t linearVertAdvance;
    GuestVector advance;
    std::uint32_t format;
    GuestBitmap bitmap;
    std::int32_t bitmap_left;
    std::int32_t bitmap_top;
    GuestOutline outline;
    std::uint32_t num_subglyphs;
    void* subglyphs;
    void* control_data;
    std::int64_t control_len;
    std::int64_t lsb_delta;
    std::int64_t rsb_delta;
    void* other;
    void* internal;
};

struct GuestFaceRec {
    std::int64_t num_faces;
    std::int64_t face_index;
    std::int64_t face_flags;
    std::int64_t style_flags;
    std::int64_t num_glyphs;
    char* family_name;
    char* style_name;
    std::int32_t num_fixed_sizes;
    GuestBitmapSize* available_sizes;
    std::int32_t num_charmaps;
    GuestCharMapRec** charmaps;
    GuestGeneric generic;
    GuestBBox bbox;
    std::uint16_t units_per_EM;
    std::int16_t ascender;
    std::int16_t descender;
    std::int16_t height;
    std::int16_t max_advance_width;
    std::int16_t max_advance_height;
    std::int16_t underline_position;
    std::int16_t underline_thickness;
    GuestGlyphSlotRec* glyph;
    GuestSizeRec* size;
    GuestCharMapRec* charmap;
    void* driver;
    void* memory;
    void* stream;
    void* sizes_list_head;
    void* sizes_list_tail;
    GuestGeneric autohint;
    void* extensions;
    void* internal;
};

static_assert(sizeof(GuestMatrix) == 32 && offsetof(GuestMatrix, xy) == 8 && offsetof(GuestMatrix, yx) == 16 && offsetof(GuestMatrix, yy) == 24);
static_assert(sizeof(GuestGeneric) == 16 && offsetof(GuestGeneric, finalizer) == 8);
static_assert(sizeof(GuestBitmapSize) == 32 && offsetof(GuestBitmapSize, size) == 8);
static_assert(sizeof(GuestCharMapRec) == 16 && offsetof(GuestCharMapRec, platform_id) == 12);
static_assert(sizeof(GuestSfntName) == 24 && offsetof(GuestSfntName, string) == 8 && offsetof(GuestSfntName, string_len) == 16);
static_assert(sizeof(GuestSizeMetrics) == 56 && offsetof(GuestSizeMetrics, x_scale) == 8);
static_assert(sizeof(GuestSizeRec) == 88 && offsetof(GuestSizeRec, metrics) == 24);
static_assert(sizeof(GuestSizeRequestRec) == 32 && offsetof(GuestSizeRequestRec, width) == 8 && offsetof(GuestSizeRequestRec, height) == 16);
static_assert(offsetof(GuestSizeRequestRec, horiResolution) == 24 && offsetof(GuestSizeRequestRec, vertResolution) == 28);
static_assert(sizeof(GuestBitmap) == 40 && offsetof(GuestBitmap, buffer) == 16 && offsetof(GuestBitmap, palette) == 32);
static_assert(sizeof(GuestOutlineFuncs) == 48 && offsetof(GuestOutlineFuncs, shift) == 32 && offsetof(GuestOutlineFuncs, delta) == 40);
static_assert(sizeof(GuestOutline) == 40 && offsetof(GuestOutline, points) == 8 && offsetof(GuestOutline, flags) == 32);
static_assert(offsetof(GuestGlyphSlotRec, glyph_index) == 0x18 && offsetof(GuestGlyphSlotRec, metrics) == 0x30);
static_assert(offsetof(GuestGlyphSlotRec, advance) == 0x80 && offsetof(GuestGlyphSlotRec, format) == 0x90);
static_assert(offsetof(GuestGlyphSlotRec, bitmap) == 0x98 && offsetof(GuestGlyphSlotRec, bitmap_left) == 0xc0 && offsetof(GuestGlyphSlotRec, bitmap_top) == 0xc4);
static_assert(offsetof(GuestGlyphSlotRec, outline) == 0xc8 && offsetof(GuestGlyphSlotRec, num_subglyphs) == 0xf0);
static_assert(offsetof(GuestGlyphSlotRec, lsb_delta) == 0x110 && offsetof(GuestGlyphSlotRec, internal) == 0x128 && sizeof(GuestGlyphSlotRec) == 0x130);
static_assert(offsetof(GuestFaceRec, num_fixed_sizes) == 56 && offsetof(GuestFaceRec, available_sizes) == 64 && offsetof(GuestFaceRec, charmaps) == 80);
static_assert(offsetof(GuestFaceRec, bbox) == 104 && offsetof(GuestFaceRec, units_per_EM) == 136 && offsetof(GuestFaceRec, underline_thickness) == 150);
static_assert(offsetof(GuestFaceRec, glyph) == 0x98 && offsetof(GuestFaceRec, size) == 0xa0 && offsetof(GuestFaceRec, charmap) == 0xa8);
static_assert(offsetof(GuestFaceRec, autohint) == 216 && offsetof(GuestFaceRec, internal) == 240 && sizeof(GuestFaceRec) == 248);

}

#endif
