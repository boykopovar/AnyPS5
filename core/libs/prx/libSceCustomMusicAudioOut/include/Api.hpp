#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicAudioOutFinalize();
int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume();
int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime();
int APS5_VABI sceCustomMusicAudioOutGetMuted();
int APS5_VABI sceCustomMusicAudioOutGetVolume();
int APS5_VABI sceCustomMusicAudioOutInitialize();
int APS5_VABI sceCustomMusicAudioOutOutput();
int APS5_VABI sceCustomMusicAudioOutSetAllMusicVolume();
int APS5_VABI sceCustomMusicAudioOutSetMuted();
int APS5_VABI sceCustomMusicAudioOutSetVolume();

}
