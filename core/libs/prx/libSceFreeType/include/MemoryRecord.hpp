#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include <cstdint>

extern "C" {

FreeType::GuestMemoryRec* APS5_VABI FT_New_Memory_nid_postfix();
void APS5_VABI FT_Done_Memory_nid_postfix(FreeType::GuestMemoryRec* memory);
int APS5_VABI sceFreeTypeOtNewMemory_nid_no_patch();

}
