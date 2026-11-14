#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI sceShareEventCallbackProcessed();
int APS5_VABI sceShareFeaturePermitCompatible(std::int32_t id);
int APS5_VABI sceShareFeatureProhibitCompatible(std::int32_t id);
int APS5_VABI sceShareGetContentParam(std::int32_t id, void* argument2, std::size_t size, void* argument4);
int APS5_VABI sceShareGetStoredAppStatus(std::int32_t id, std::uint32_t argument2, void* status);
int APS5_VABI sceShareInitializeForShellCore();
int APS5_VABI sceShareInitializeInternal();
int APS5_VABI sceShareRegisterEventCallback(std::int32_t argument1, std::uint32_t argument2, const void* callback);
int APS5_VABI sceShareSetCurrentStatus(std::int32_t id, std::uint32_t argument2, const void* status);
int APS5_VABI sceShareSetUploadEventInfo(std::int32_t argument1, const void* argument2, const void* argument3);
int APS5_VABI sceShareTerminateForShellCore();
int APS5_VABI sceShareTerminateInternal();
int APS5_VABI sceShareUnregisterEventCallback(const void* callback);
int APS5_VABI libSceShareInternal_nativeUnknown__plus_LB32s0kCYg_nid_no_patch(std::int32_t id);
int APS5_VABI libSceShareInternal_nativeUnknown_0IbPXX_minus_WvQQ_nid_no_patch(std::int32_t argument1, const void* argument2);
int APS5_VABI libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(std::int32_t id, void* argument2, void* argument3, void* argument4);
int APS5_VABI libSceShareInternal_nativeUnknown_9fIEK6i1idg_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_9oywh7VdSjM_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_IvkDssl52mA_nid_no_patch(std::int32_t argument1, void* argument2);
int APS5_VABI libSceShareInternal_nativeUnknown_zyzD8P5ZKPA_nid_no_patch(const char* name, void* argument2, std::size_t size, void* argument4);

}
