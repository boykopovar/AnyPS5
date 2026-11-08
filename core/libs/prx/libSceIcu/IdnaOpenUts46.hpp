#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UIDNA;

extern "C" {
UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* pErrorCode);
}
