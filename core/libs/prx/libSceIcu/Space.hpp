#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int8_t APS5_VABI u_isspace_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isblank_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isWhitespace_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isJavaSpaceChar_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isUWhiteSpace_nid_postfix(std::int32_t);
}
