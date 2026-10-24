#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2GetTimestamp();
int APS5_VABI sceSystemLogger2Initialize();
int APS5_VABI sceSystemLogger2SetEventAsync();
int APS5_VABI sceSystemLogger2SetEventAsyncEx();
int APS5_VABI sceSystemLogger2Terminate();

}
