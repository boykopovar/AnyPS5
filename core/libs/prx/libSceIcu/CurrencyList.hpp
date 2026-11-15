#pragma once
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
UEnumeration* APS5_VABI ucurr_openISOCurrencies(std::uint32_t type, std::int32_t* status);
}
