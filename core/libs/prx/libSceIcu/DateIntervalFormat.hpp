#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UDateIntervalFormat;

extern "C" {
void APS5_VABI udtitvfmt_close_nid_postfix(UDateIntervalFormat* formatter);
}
