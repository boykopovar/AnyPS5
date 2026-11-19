#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI udata_setCommonData(const void*, std::int32_t*);
void APS5_VABI udata_setFileAccess(std::int32_t, std::int32_t*);
}
