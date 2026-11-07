// Derived from FreeType include/freetype/internal/ftgloadr.h (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2002, 2003, 2005, 2006 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
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

struct GuestGlyphLoadRec {
    GuestOutline outline;
    GuestVector* extra_points;
    GuestVector* extra_points2;
    std::uint32_t num_subglyphs;
    GuestSubGlyphRec* subglyphs;
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

static_assert(sizeof(GuestSubGlyphRec) == 48 && offsetof(GuestSubGlyphRec, flags) == 4 && offsetof(GuestSubGlyphRec, arg1) == 8);
static_assert(offsetof(GuestSubGlyphRec, arg2) == 12 && offsetof(GuestSubGlyphRec, transform) == 16);
static_assert(sizeof(GuestGlyphLoadRec) == 72 && offsetof(GuestGlyphLoadRec, extra_points) == 40 && offsetof(GuestGlyphLoadRec, extra_points2) == 48);
static_assert(offsetof(GuestGlyphLoadRec, num_subglyphs) == 56 && offsetof(GuestGlyphLoadRec, subglyphs) == 64);
static_assert(sizeof(GuestGlyphLoaderRec) == 176 && offsetof(GuestGlyphLoaderRec, max_points) == 8 && offsetof(GuestGlyphLoaderRec, max_contours) == 12);
static_assert(offsetof(GuestGlyphLoaderRec, max_subglyphs) == 16 && offsetof(GuestGlyphLoaderRec, use_extra) == 20);
static_assert(offsetof(GuestGlyphLoaderRec, base) == 24 && offsetof(GuestGlyphLoaderRec, current) == 96 && offsetof(GuestGlyphLoaderRec, other) == 168);

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
