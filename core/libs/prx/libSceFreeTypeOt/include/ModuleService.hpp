/***************************************************************************/
/*                                                                         */
/*  ftobjs.h                                                               */
/*                                                                         */
/*    The FreeType private base classes (specification).                   */
/*                                                                         */
/*  Copyright 1996-2006, 2008, 2010, 2012 by                               */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/
// https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/include/freetype/internal/ftobjs.h

#pragma once

#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"

namespace FreeType {

struct GuestServiceLibraryRec;

struct GuestServiceModuleRec {
    GuestModuleClass* clazz;
    GuestServiceLibraryRec* library;
    GuestMemoryRec* memory;
};

struct GuestServiceLibraryRec {
    GuestMemoryRec* memory;
    std::int32_t versionMajor;
    std::int32_t versionMinor;
    std::int32_t versionPatch;
    std::uint32_t numModules;
    GuestServiceModuleRec* modules[32];
};

using GuestServiceRequester = void* (APS5_VABI *)(GuestServiceModuleRec*, const char*);

static_assert(sizeof(GuestServiceModuleRec) == 24 && offsetof(GuestServiceModuleRec, library) == 8);
static_assert(offsetof(GuestServiceLibraryRec, numModules) == 20 && offsetof(GuestServiceLibraryRec, modules) == 24);

}

extern "C" void* APS5_VABI ft_module_get_service_nid_postfix(FreeType::GuestServiceModuleRec* module, const char* serviceId);
