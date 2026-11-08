#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UIDNA;

extern "C" {
void APS5_VABI uidna_close_nid_postfix(UIDNA* idna);
}
