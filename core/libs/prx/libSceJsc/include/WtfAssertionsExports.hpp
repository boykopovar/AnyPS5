#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

void APS5_VABI WTFLogAlwaysV(const char* format, VaList* args);

}
