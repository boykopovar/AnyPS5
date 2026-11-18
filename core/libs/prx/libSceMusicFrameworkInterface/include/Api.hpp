#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMusicFwGetCurrentTime(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetLoop(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetMuted(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetNetworkState(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwGetVolume(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwIsEnded(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwIsPlaying(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwIsStopped(std::int32_t handle, void* output);
int APS5_VABI sceMusicFwRemoveSrc();
int APS5_VABI sceMusicFwSendMessageAsynchronously();
int APS5_VABI sceMusicFwSendMessageSynchronously();
int APS5_VABI sceMusicFwSetCurrentTime();
int APS5_VABI sceMusicFwSetLoop();
int APS5_VABI sceMusicFwSetShuffle();
int APS5_VABI sceMusicFwSetVolume();
int APS5_VABI sceMusicFwSkipNext();
int APS5_VABI sceMusicFwSkipPrevious();

}
