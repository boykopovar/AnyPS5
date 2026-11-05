#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrHandDebugGetConfig_nid_no_patch(void);
int APS5_VABI sceVrHandDebugGetInternalResult_nid_no_patch(void);
int APS5_VABI sceVrHandDebugSetConfig_nid_no_patch(void);
int APS5_VABI sceVrHandSetDebugInterface_nid_no_patch(void);

}
