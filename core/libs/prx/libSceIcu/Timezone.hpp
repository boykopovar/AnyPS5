#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI uprv_tzset_nid_postfix();
const char* APS5_VABI uprv_tzname_nid_postfix(std::int32_t index);
std::int32_t APS5_VABI uprv_timezone_nid_postfix();
}
