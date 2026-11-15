#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCameraGetUsageInfo();
int APS5_VABI sceCameraGetUsageInfoWithoutCountProcess();
int APS5_VABI sceCameraOpenByModuleIdWithMode();
int APS5_VABI sceCameraSetForceDriverMode();
int APS5_VABI sceCameraIsVrCaptureEnable();

}

extern "C" {
int APS5_VABI libSceCameraUnknown_AnqFx_minus_nAxZM_nid_no_patch();
int APS5_VABI libSceCameraUnknown_azeU8qML4VM_nid_no_patch();
int APS5_VABI libSceCameraUnknown_kgRnfleNYNo_nid_no_patch();
}
