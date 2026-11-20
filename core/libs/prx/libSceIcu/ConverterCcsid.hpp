#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

extern "C" {
UConverter* APS5_VABI ucnv_openCCSID_nid_postfix(std::int32_t codepage, std::int32_t platform, std::int32_t* status);
}
