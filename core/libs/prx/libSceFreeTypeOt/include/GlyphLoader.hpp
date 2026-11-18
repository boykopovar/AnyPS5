// Derived from FreeType include/freetype/internal/ftgloadr.h (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2002, 2003, 2005, 2006 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestSubGlyphRec {
    std::int32_t index;
    std::uint16_t flags;
    std::int32_t arg1;
    std::int32_t arg2;
    GuestMatrix transform;
};

static_assert(sizeof(GuestSubGlyphRec) == 48 && offsetof(GuestSubGlyphRec, flags) == 4 && offsetof(GuestSubGlyphRec, arg1) == 8);
static_assert(offsetof(GuestSubGlyphRec, arg2) == 12 && offsetof(GuestSubGlyphRec, transform) == 16);

}

extern "C" {

std::int32_t APS5_VABI FT_GlyphLoader_New_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestGlyphLoaderRec** aloader);
std::int32_t APS5_VABI FT_GlyphLoader_CreateExtra_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
void APS5_VABI FT_GlyphLoader_Done_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
void APS5_VABI FT_GlyphLoader_Reset_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
void APS5_VABI FT_GlyphLoader_Rewind_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
std::int32_t APS5_VABI FT_GlyphLoader_CheckPoints_nid_postfix(FreeType::GuestGlyphLoaderRec* loader, std::uint32_t n_points, std::uint32_t n_contours);
std::int32_t APS5_VABI FT_GlyphLoader_CheckSubGlyphs_nid_postfix(FreeType::GuestGlyphLoaderRec* loader, std::uint32_t n_subs);
void APS5_VABI FT_GlyphLoader_Prepare_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
void APS5_VABI FT_GlyphLoader_Add_nid_postfix(FreeType::GuestGlyphLoaderRec* loader);
std::int32_t APS5_VABI FT_GlyphLoader_CopyPoints_nid_postfix(FreeType::GuestGlyphLoaderRec* target, FreeType::GuestGlyphLoaderRec* source);

}
