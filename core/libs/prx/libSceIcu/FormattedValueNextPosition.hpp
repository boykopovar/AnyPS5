#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFormattedValue;
struct UConstrainedFieldPosition;

extern "C" {
std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, std::int32_t* ec);
}
