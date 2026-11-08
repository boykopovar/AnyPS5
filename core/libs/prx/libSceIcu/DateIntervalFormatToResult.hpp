#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateIntervalFormat;
struct UFormattedDateInterval;

extern "C" {
void APS5_VABI udtitvfmt_formatToResult_nid_postfix(const UDateIntervalFormat* formatter, double fromDate, double toDate, UFormattedDateInterval* result, std::int32_t* status);
}
