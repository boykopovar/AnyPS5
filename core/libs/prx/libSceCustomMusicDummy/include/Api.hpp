#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicDummyFileCheckReachability(void);
int APS5_VABI sceCustomMusicDummyKernelMmap(void);
int APS5_VABI sceCustomMusicDummyFileOpen(void);

}
