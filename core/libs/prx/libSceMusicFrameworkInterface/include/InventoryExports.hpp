#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMusicFwAddSrc();
int APS5_VABI sceMusicFwFinalize();
int APS5_VABI sceMusicFwGetCapability(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetDuration(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetShuffle(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetVolume2(void* output);
int APS5_VABI sceMusicFwInitialize();
int APS5_VABI sceMusicFwInitialize2(const void* param);
int APS5_VABI sceMusicFwIsPaused(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwIsSeeking(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwLoad();
int APS5_VABI sceMusicFwPause();
int APS5_VABI sceMusicFwPlay();
int APS5_VABI sceMusicFwSetMuted();
int APS5_VABI sceMusicFwSetVolume2();
int APS5_VABI sceMusicFwStop();
int APS5_VABI libSceMusicFrameworkInterfaceUnknown_X9GN6gWabWM_nid_no_patch(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwRegisterNotifyMessage();
int APS5_VABI sceMusicFwUnregisterNotifyMessage();
int APS5_VABI sceMusicFwAbortMessage();

}
