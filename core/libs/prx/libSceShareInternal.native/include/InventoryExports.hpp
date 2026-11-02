#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceShareEventCallbackProcessed();
int APS5_VABI sceShareFeaturePermitCompatible();
int APS5_VABI sceShareFeatureProhibitCompatible();
int APS5_VABI sceShareGetContentParam();
int APS5_VABI sceShareGetStoredAppStatus();
int APS5_VABI sceShareInitializeForShellCore();
int APS5_VABI sceShareInitializeInternal();
int APS5_VABI sceShareRegisterEventCallback();
int APS5_VABI sceShareSetCurrentStatus();
int APS5_VABI sceShareSetUploadEventInfo();
int APS5_VABI sceShareTerminateForShellCore();
int APS5_VABI sceShareTerminateInternal();
int APS5_VABI sceShareUnregisterEventCallback();
int APS5_VABI libSceShareInternal_nativeUnknown__plus_LB32s0kCYg_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_0IbPXX_minus_WvQQ_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_9fIEK6i1idg_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_9oywh7VdSjM_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_IvkDssl52mA_nid_no_patch();
int APS5_VABI libSceShareInternal_nativeUnknown_zyzD8P5ZKPA_nid_no_patch();

}
