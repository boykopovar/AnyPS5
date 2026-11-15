// Derived from FreeType include/freetype/internal/ftrfork.h (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2004, 2006, 2007, 2012 by Masatake YAMATO and Redhat K.K.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#pragma once

#include "prx/libSceFreeTypeOt/include/Stream.hpp"

#include <cstdint>

namespace FreeType {

struct GuestRaccessLibrary {
    GuestMemoryRec* memory;
};

}

extern "C" {

int APS5_VABI FT_Raccess_Get_HeaderInfo_nid_postfix(FreeType::GuestRaccessLibrary* library, FreeType::GuestStreamRec* stream, std::int64_t rforkOffset,
                                                    std::int64_t* mapOffset, std::int64_t* rdataPos);
int APS5_VABI FT_Raccess_Get_DataOffsets_nid_postfix(FreeType::GuestRaccessLibrary* library, FreeType::GuestStreamRec* stream, std::int64_t mapOffset,
                                                     std::int64_t rdataPos, std::int64_t tag, std::int64_t** offsets, std::int64_t* count);
void APS5_VABI FT_Raccess_Guess_nid_postfix(FreeType::GuestRaccessLibrary* library, FreeType::GuestStreamRec* stream, char* baseName, char** newNames,
                                           std::int64_t* offsets, std::int32_t* errors);

}
