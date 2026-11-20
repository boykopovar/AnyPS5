#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const void* resourceData, std::uint32_t resource);
}
