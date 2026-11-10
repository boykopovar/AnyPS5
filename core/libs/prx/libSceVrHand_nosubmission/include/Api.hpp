#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrHandDebugGetConfig(void);
int APS5_VABI sceVrHandDebugGetInternalResult(void);
int APS5_VABI sceVrHandDebugSetConfig(void);
int APS5_VABI sceVrHandSetDebugInterface(void);

}
