// Derived from FreeType include/freetype/internal/ftobjs.h (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
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

}

extern "C" {

void APS5_VABI ft_glyphslot_free_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot);
void APS5_VABI ft_glyphslot_set_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::uint8_t* buffer);
int APS5_VABI ft_glyphslot_alloc_bitmap_nid_postfix(FreeType::GuestGlyphSlotRec* slot, std::uint64_t size);

}
