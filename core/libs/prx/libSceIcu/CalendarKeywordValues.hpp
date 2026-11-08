#pragma once
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t*);
}
