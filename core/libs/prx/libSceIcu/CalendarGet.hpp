#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCalendar;
using UCalendarDateFields = std::int32_t;

extern "C" {
std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar* cal, UCalendarDateFields field, std::int32_t* status);
}
