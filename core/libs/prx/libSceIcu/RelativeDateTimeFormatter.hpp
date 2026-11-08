#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct URelativeDateTimeFormatter;

extern "C" {
void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt);
}
