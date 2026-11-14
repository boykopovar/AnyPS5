#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI u_terminateChars_nid_postfix(char* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status);
std::int32_t APS5_VABI u_terminateUChar32s_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status);
}
