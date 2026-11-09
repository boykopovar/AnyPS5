#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"

#include <cstdint>

extern "C" {

int APS5_VABI FT_Outline_New_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint32_t numPoints, std::int32_t numContours, FreeType::GuestOutline* anoutline);
int APS5_VABI FT_Outline_Done_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestOutline* outline);

}
