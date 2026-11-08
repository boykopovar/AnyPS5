#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UPluralRules;

extern "C" {
void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules);
}
