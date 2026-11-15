#pragma once

#include <cstdint>

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceIduGetDiscTitleInfo(void* info, std::uint32_t count);
int APS5_VABI sceIduGetFsStats(void* stats);
int APS5_VABI sceIduGetHardwareId(void* id);
int APS5_VABI sceIduGetPkgInstallStatus();
int APS5_VABI sceIduGetTitleList(void* list, std::int32_t count);
int APS5_VABI sceIduInstallRif(const char* name, const void* data, std::uint32_t size);
int APS5_VABI sceIduNotificationHide(const char* id);
int APS5_VABI sceIduNotificationShow();
int APS5_VABI sceIduStartTitle(const void* title, std::int32_t count, const void* items, const void* param);
int APS5_VABI libSceIduUnknown_3XKI6v75VEo_nid_no_patch();
int APS5_VABI libSceIduUnknown_J9y431a1uM0_nid_no_patch();
int APS5_VABI libSceIduUnknown_tmeq5nZRqIs_nid_no_patch();
int APS5_VABI libSceIduUnknown_uoHZCJW8Cdk_nid_no_patch(void* out);

}
