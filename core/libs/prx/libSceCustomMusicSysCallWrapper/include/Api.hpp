#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicNetInetNtop();
int APS5_VABI sceCustomMusicNetShutdown();
int APS5_VABI sceCustomMusicNetSocketAbort();
int APS5_VABI sceCustomMusicPthreadAttrGet();
int APS5_VABI sceCustomMusicPthreadSelf();
int APS5_VABI sceCustomMusicSelect();

}
