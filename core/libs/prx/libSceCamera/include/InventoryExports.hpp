#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCameraGetUsageInfo(std::int32_t userId, void* info);
int APS5_VABI sceCameraGetUsageInfoWithoutCountProcess(std::int32_t userId, void* info);
int APS5_VABI sceCameraOpenByModuleIdWithMode(std::int32_t userId, std::int32_t type, std::int32_t index, void* param, std::uint32_t moduleId, std::uint32_t mode);
int APS5_VABI sceCameraSetForceDriverMode(std::int32_t mode);
int APS5_VABI sceCameraIsVrCaptureEnable(std::int32_t* enabled);

}

extern "C" {
int APS5_VABI libSceCameraUnknown_AnqFx_minus_nAxZM_nid_no_patch(void);
int APS5_VABI libSceCameraUnknown_azeU8qML4VM_nid_no_patch(void* info);
int APS5_VABI libSceCameraUnknown_kgRnfleNYNo_nid_no_patch(void* info);
}
