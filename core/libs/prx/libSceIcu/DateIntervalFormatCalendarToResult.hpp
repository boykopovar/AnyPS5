#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCalendar;
struct UDateIntervalFormat;
struct UFormattedDateInterval;

extern "C" {
void APS5_VABI udtitvfmt_formatCalendarToResult_nid_postfix(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval*, std::int32_t*);
}
