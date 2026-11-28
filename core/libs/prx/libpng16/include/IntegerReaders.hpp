#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

std::uint16_t APS5_VABI png_get_uint_16_nid_postfix(const std::uint8_t* bytes);
std::uint32_t APS5_VABI png_get_uint_32_nid_postfix(const std::uint8_t* bytes);

}
