#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI udata_setCommonData_nid_postfix(const void* data, std::int32_t* status);
void APS5_VABI udata_setFileAccess_nid_postfix(std::int32_t access, std::int32_t* status);
}
