#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

struct UCalendar;

extern "C" {
void APS5_VABI ucal_close_nid_postfix(UCalendar* cal);
}
