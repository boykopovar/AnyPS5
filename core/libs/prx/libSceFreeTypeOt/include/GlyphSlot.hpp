// Derived from FreeType include/freetype/internal/ftobjs.h, ftdriver.h, ftgloadr.h and include/freetype/ftmodapi.h (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 1996-2006, 2008, 2010, 2012 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

constexpr std::uint32_t GlyphOwnBitmap = 0x1;
constexpr std::uint64_t ModuleDriverNoOutlines = 0x200;

struct GuestModuleClass {
    std::uint64_t module_flags;
    std::int64_t module_size;
    const char* module_name;
    std::int64_t module_version;
    std::int64_t module_requires;
    const void* module_interface;
    void* module_init;
    void* module_done;
    void* get_interface;
};

struct GuestDriverClassPrefix {
    GuestModuleClass root;
    std::int64_t face_object_size;
    std::int64_t size_object_size;
    std::int64_t slot_object_size;
    void* init_face;
    void* done_face;
    void* init_size;
    void* done_size;
    int (APS5_VABI *init_slot)(GuestGlyphSlotRec*);
    void (APS5_VABI *done_slot)(GuestGlyphSlotRec*);
};

struct GuestDriverRec {
    GuestModuleClass* root_clazz;
    void* root_library;
    GuestMemoryRec* root_memory;
    GuestDriverClassPrefix* clazz;
    GuestListRec faces_list;
    void* glyph_loader;
};

struct GuestGlyphLoadRec {
    GuestOutline outline;
    GuestVector* extra_points;
    GuestVector* extra_points2;
    std::uint32_t num_subglyphs;
    void* subglyphs;
};

struct GuestGlyphLoaderRec {
    GuestMemoryRec* memory;
    std::uint32_t max_points;
    std::uint32_t max_contours;
    std::uint32_t max_subglyphs;
    std::uint8_t use_extra;
    GuestGlyphLoadRec base;
    GuestGlyphLoadRec current;
    void* other;
};

struct GuestSlotInternalRec {
    void* loader;
    std::uint32_t flags;
    std::uint8_t glyph_transformed;
    GuestMatrix glyph_matrix;
    GuestVector glyph_delta;
    void* glyph_hints;
};

static_assert(sizeof(GuestSlotInternalRec) == 72 && offsetof(GuestSlotInternalRec, flags) == 8 && offsetof(GuestSlotInternalRec, glyph_transformed) == 12);
static_assert(offsetof(GuestSlotInternalRec, glyph_matrix) == 16 && offsetof(GuestSlotInternalRec, glyph_delta) == 48 && offsetof(GuestSlotInternalRec, glyph_hints) == 64);
static_assert(offsetof(GuestFaceRec, memory) == 0xb8);
static_assert(sizeof(GuestModuleClass) == 72 && offsetof(GuestModuleClass, module_name) == 16 && offsetof(GuestModuleClass, get_interface) == 64);
static_assert(offsetof(GuestDriverClassPrefix, slot_object_size) == 88 && offsetof(GuestDriverClassPrefix, init_slot) == 128 && offsetof(GuestDriverClassPrefix, done_slot) == 136);
static_assert(sizeof(GuestDriverRec) == 56 && offsetof(GuestDriverRec, root_memory) == 16 && offsetof(GuestDriverRec, clazz) == 24 && offsetof(GuestDriverRec, glyph_loader) == 48);
static_assert(sizeof(GuestGlyphLoadRec) == 72 && offsetof(GuestGlyphLoadRec, extra_points) == 40 && offsetof(GuestGlyphLoadRec, subglyphs) == 64);
static_assert(sizeof(GuestGlyphLoaderRec) == 176 && offsetof(GuestGlyphLoaderRec, use_extra) == 20 && offsetof(GuestGlyphLoaderRec, base) == 24);
static_assert(offsetof(GuestGlyphLoaderRec, current) == 96 && offsetof(GuestGlyphLoaderRec, other) == 168);
static_assert(offsetof(GuestFaceRec, glyph) == 0x98 && offsetof(GuestFaceRec, driver) == 0xb0);

}

extern "C" {

void APS5_VABI ft_glyphslot_free_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot);
void APS5_VABI ft_glyphslot_set_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::uint8_t* buffer);
int APS5_VABI ft_glyphslot_alloc_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::uint64_t size);
int APS5_VABI FT_New_GlyphSlot_nid_postfix(FreeType::GuestFaceRec* face, FreeType::GuestGlyphSlotRec** aslot);
int APS5_VABI FT_Render_Glyph_Internal_nid_postfix(FreeType::GuestLibrary* library, FreeType::GuestGlyphSlotRec* slot, std::int32_t renderMode);

}
