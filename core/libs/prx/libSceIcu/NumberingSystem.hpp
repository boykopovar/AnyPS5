#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UNumberingSystem;

extern "C" {
void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys);
}
