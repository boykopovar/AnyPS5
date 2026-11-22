#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCustomMusicAudioOutFinalize(void);
int APS5_VABI sceCustomMusicAudioOutGetAllMusicVolume(double* out);
int APS5_VABI sceCustomMusicAudioOutGetLastOutputTime(std::uint64_t* out);
int APS5_VABI sceCustomMusicAudioOutGetMuted(bool* out);
int APS5_VABI sceCustomMusicAudioOutGetVolume(double* out);
int APS5_VABI sceCustomMusicAudioOutInitialize(int frames, int attribute, int format);
int APS5_VABI sceCustomMusicAudioOutOutput(const void* buf);
int APS5_VABI sceCustomMusicAudioOutSetAllMusicVolume(double v);
int APS5_VABI sceCustomMusicAudioOutSetMuted(bool m);
int APS5_VABI sceCustomMusicAudioOutSetVolume(double v);

}
