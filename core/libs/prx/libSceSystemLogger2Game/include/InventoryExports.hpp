#pragma once

#include "prx/libSceSystemLogger2/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2GameGetTimestamp(SceSystemLogger2Timestamp* timestamp);
int APS5_VABI sceSystemLogger2GameSetEvent();

}
