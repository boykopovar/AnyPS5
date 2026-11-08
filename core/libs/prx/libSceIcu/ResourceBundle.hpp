#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

struct UResourceBundle;

extern "C" {

void APS5_VABI ures_close(UResourceBundle* resB);

}
