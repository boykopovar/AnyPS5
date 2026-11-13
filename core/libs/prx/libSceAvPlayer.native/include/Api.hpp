#pragma once

#include "SceTypes.hpp"
#include <cstdarg>

extern "C" {

int APS5_VABI sceAvPlayerPrintf(const char* format, ...);
int APS5_VABI sceAvPlayerVprintf(const char* format, VaList* arguments);

}
