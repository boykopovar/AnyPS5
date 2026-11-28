#pragma once

#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicAudioOutFinalize(void);
int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume(double* out);
int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime(void* out);
int APS5_VABI sceCustomMusicAudioOutGetMuted(bool* out);
int APS5_VABI sceCustomMusicAudioOutGetVolume(double* out);
int APS5_VABI sceCustomMusicAudioOutInitialize(int a1, int a2, int a3);
int APS5_VABI sceCustomMusicAudioOutOutput(void* buf);
int APS5_VABI sceCustomMusicAudioOutSetAllMusicVolume(double v);
int APS5_VABI sceCustomMusicAudioOutSetMuted(bool m);
int APS5_VABI sceCustomMusicAudioOutSetVolume(double v);

}
