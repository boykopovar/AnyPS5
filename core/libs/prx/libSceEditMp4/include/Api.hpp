#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEditMp4Abort();
int APS5_VABI sceEditMp4Edit();
int APS5_VABI sceEditMp4GetStatus();
int APS5_VABI sceEditMp4Initialize();
int APS5_VABI sceEditMp4InitializeParam();
int APS5_VABI sceEditMp4QueryMemSize();
int APS5_VABI sceEditMp4SetFinishCallback();
int APS5_VABI sceEditMp4Terminate();
int APS5_VABI sceEditMp4Unite();

}
