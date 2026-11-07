#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2GetTimestamp();
int APS5_VABI sceSystemLogger2Initialize();
int APS5_VABI sceSystemLogger2SetEventAsync();
int APS5_VABI sceSystemLogger2SetEventAsyncEx();
int APS5_VABI sceSystemLogger2Terminate();

int APS5_VABI sceSystemLogger2GetEventStatus();
int APS5_VABI sceSystemLogger2IsAvailableMonitor();
int APS5_VABI sceSystemLogger2TerminateWithResourceRelease();
int APS5_VABI sceSystemLogger2SetEventAsyncEx2();
int APS5_VABI sceSystemLogger2SetRnpsEventAsync();
int APS5_VABI sceSystemLogger2SetEventAsyncInternal();

}
