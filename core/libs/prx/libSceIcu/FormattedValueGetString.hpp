#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFormattedValue;

extern "C" {
const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength, std::int32_t* ec);
}
