#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceContentSearchAddAvContentCreatedEventCondition(std::int32_t first, std::int32_t second);
int APS5_VABI sceContentSearchDisableAvContentCreatedEvent(void);
int APS5_VABI sceContentSearchEnableAvContentCreatedEvent(void);
int APS5_VABI sceContentSearchGetContentIdByAvContentCreatedEvent(const void* event, std::uint64_t* contentId);

int APS5_VABI libSceContentSearchUnknown_Np8cEpyDFRE_nid_no_patch();
int APS5_VABI libSceContentSearchUnknown_X9e_plus_hZfue0U_nid_no_patch();

}
