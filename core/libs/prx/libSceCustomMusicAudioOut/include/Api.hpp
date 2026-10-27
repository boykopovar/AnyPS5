#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicAudioOutFinalize();
int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume();
int APS5_VABI sceCustomMusicAudioOutGetMuted();
int APS5_VABI sceCustomMusicAudioOutGetVolume();
int APS5_VABI sceCustomMusicAudioOutSetAllMusicVolume();

}
