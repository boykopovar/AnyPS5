#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceVcodecCloseHwBudget(std::uint32_t budget);
int APS5_VABI sceVcodecOpenHwBudgetByAppid(std::uint32_t appId, std::uint32_t value);
int APS5_VABI sceVcodecSetHwBudgetSize(std::uint32_t budget, std::uint32_t size);
int APS5_VABI sceVcodecOpenHwBudget(std::int32_t pid, std::uint32_t value);
int APS5_VABI sceVcodecSetHwBudgetType(std::uint32_t budget, std::uint32_t first, std::uint32_t second);
int APS5_VABI libSceVcodecUnknown_X0qscgxx1MI_nid_no_patch(std::uint32_t budget, std::int32_t pid);
int APS5_VABI libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(std::uint64_t* result, const void* config);
int APS5_VABI libSceVcodecUnknown_Ac6aYjr1pY8_nid_no_patch(void* output);

}
