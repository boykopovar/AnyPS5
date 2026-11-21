#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

extern "C" {

int APS5_VABI freetypeLegacyLibrarySetLcdGeometry_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestVector* sub);
int APS5_VABI libfreetypeGetVarAxisFlags_nid_no_patch(void* master, std::uint32_t axisIndex, std::uint32_t* flags);

}
