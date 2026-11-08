#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCalendar;

extern "C" {
UCalendar* APS5_VABI ucal_clone_nid_postfix(const UCalendar* cal, std::int32_t* status);
}
