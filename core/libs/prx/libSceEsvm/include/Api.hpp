#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmAppAudioSyncCreateContext();
int APS5_VABI sceEsvmAppAudioSyncFlush();
int APS5_VABI sceEsvmSyncVideo();

}
