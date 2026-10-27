#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicCoreBgmClose();
int APS5_VABI sceCustomMusicCoreBgmOpen();
int APS5_VABI sceCustomMusicCoreBgmOpen2();
int APS5_VABI sceCustomMusicCoreBgmOutput();
int APS5_VABI sceCustomMusicCoreBgmSetAudioVolume();
int APS5_VABI sceCustomMusicCoreBgmStart();
int APS5_VABI sceCustomMusicCoreBgmStop();
int APS5_VABI sceCustomMusicCoreGetBgmAuthorityStatus();
int APS5_VABI sceCustomMusicCoreGetHdcpStatus();
int APS5_VABI sceCustomMusicCoreGetSystemAudioVolume();
int APS5_VABI sceCustomMusicCorePostEvent();
int APS5_VABI sceCustomMusicCoreSendEvent();
int APS5_VABI sceCustomMusicCoreSendMulticastEvent();
int APS5_VABI sceCustomMusicCoreSendSpMessage();
int APS5_VABI sceCustomMusicCoreSetSystemAudioVolume();
int APS5_VABI sceCustomMusicCoreStartOperation();
int APS5_VABI sceCustomMusicCoreStartOperation2();
int APS5_VABI sceCustomMusicCoreStopOperation();

}
