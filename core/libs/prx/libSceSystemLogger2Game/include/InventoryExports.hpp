#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2GameGetTimestamp(void* timestamp);
int APS5_VABI sceSystemLogger2GameSetEvent(const void* event);

}
