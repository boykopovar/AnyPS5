#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrHandDebugSetConfig();
int APS5_VABI sceVrHandSetDebugInterface();
int APS5_VABI sceVrHandDebugGetConfig();
int APS5_VABI sceVrHandDebugGetInternalResult();

}
