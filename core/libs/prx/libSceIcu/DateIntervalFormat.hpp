#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDateIntervalFormat;

extern "C" {
UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status);
}
