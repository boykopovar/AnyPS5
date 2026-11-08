#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCalendar;
using UDate = double;

extern "C" {
void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* cal, UDate date, std::int32_t* status);
}
