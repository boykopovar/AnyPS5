#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

int APS5_VABI FT_Reference_Library_nid_postfix(FreeType::GuestLibrary* library);
int APS5_VABI FT_New_Library_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestLibrary** alibrary);
void* APS5_VABI FT_Lookup_Renderer_nid_postfix(FreeType::GuestLibrary* library, std::uint32_t format, void** node);

}
