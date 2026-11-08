#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UNumberFormat;

extern "C" {
void APS5_VABI unum_close_nid_postfix(UNumberFormat*);
}
