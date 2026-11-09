#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceContentDeleteInitialize(const ContentDeleteInitParam* initParam);
int APS5_VABI sceContentDeleteByPath(const char* contentPath);
int APS5_VABI sceContentDeleteById(std::int64_t contentId);
int APS5_VABI sceContentDeleteByIdListForSf(const std::int64_t* contentIds, std::size_t count);
int APS5_VABI sceContentDeleteGlobalMstat(void);
int APS5_VABI sceContentDeleteMemoryInitialize(void);
int APS5_VABI sceContentDeleteMemoryTerminate(void);
int APS5_VABI sceContentDeleteTerminate(void);

}
