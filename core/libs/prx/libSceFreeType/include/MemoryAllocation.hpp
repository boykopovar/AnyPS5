#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI FT_Alloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block);
std::int32_t APS5_VABI FT_QAlloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, void** block);
std::int32_t APS5_VABI FT_Realloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void** block);
std::int32_t APS5_VABI FT_QRealloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void** block);
void APS5_VABI FT_Free_nid_postfix(FreeType::GuestMemoryRec* memory, void** block);

}
