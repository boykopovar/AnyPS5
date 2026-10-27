#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterOnCompletion();
int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterPairOnCompletion();
int APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromTimeCounterOnCompletion();

}
