#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ulocimp_getRegionForSupplementalData_nid_postfix(const char* localeID, std::int8_t inferRegion,
                                                                        char* region, std::int32_t regionCapacity,
                                                                        std::int32_t* status);
}
