#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceShareFeatureExPermit(uint32_t feature_flags);
int APS5_VABI sceShareFeatureExProhibit(uint32_t feature_flags);
int APS5_VABI sceShareRegisterUploadCallback(void* callback, void* user_data);
int APS5_VABI sceShareUnregisterUploadCallback(void* callback);
int APS5_VABI libSceShareUnknown_YPIiGz4uKOU_nid_no_patch();
int APS5_VABI libSceShareUnknown_txixe0fiPKM_nid_no_patch();

}
