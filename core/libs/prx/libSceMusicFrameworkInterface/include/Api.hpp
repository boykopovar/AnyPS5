#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMusicFwGetCurrentTime();
int APS5_VABI sceMusicFwGetLoop();
int APS5_VABI sceMusicFwGetMuted();
int APS5_VABI sceMusicFwGetNetworkState();
int APS5_VABI sceMusicFwGetVolume();
int APS5_VABI sceMusicFwIsEnded();
int APS5_VABI sceMusicFwIsPlaying();
int APS5_VABI sceMusicFwIsStopped();
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
