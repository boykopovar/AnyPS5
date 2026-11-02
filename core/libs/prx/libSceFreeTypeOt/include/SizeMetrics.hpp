// Derived from FreeType src/base/ftobjs.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright (C) 1996-2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI FT_Select_Metrics_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t strike_index);
void APS5_VABI FT_Request_Metrics_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* req);
std::int32_t APS5_VABI FT_Match_Size_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* req, unsigned char ignore_width, std::uint64_t* size_index);

}
