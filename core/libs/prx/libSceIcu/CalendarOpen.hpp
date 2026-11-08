#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCalendar;

extern "C" {
UCalendar* APS5_VABI ucal_open_nid_postfix(const char16_t* zoneId, std::int32_t length, const char* locale, std::int32_t calendarType, std::int32_t* status);
}
