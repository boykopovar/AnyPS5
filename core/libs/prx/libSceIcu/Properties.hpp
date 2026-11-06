#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
using CharTypeRange = std::int8_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t);
} // namespace Icu

extern "C" {
std::int8_t APS5_VABI u_charType_nid_postfix(std::int32_t);
void APS5_VABI u_enumCharTypes_nid_postfix(Icu::CharTypeRange, const void*);
std::int8_t APS5_VABI u_islower_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isupper_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_istitle_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isalpha_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isalnum_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isdefined_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isbase_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_iscntrl_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_ispunct_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isgraph_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isprint_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isIDStart_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isIDPart_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isIDIgnorable_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isJavaIDStart_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isJavaIDPart_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isUAlphabetic_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isULowercase_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isUUppercase_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_charDigitValue_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_digit_nid_postfix(std::int32_t, std::int8_t);
double APS5_VABI u_getNumericValue_nid_postfix(std::int32_t);
void APS5_VABI u_charAge_nid_postfix(std::int32_t, std::uint8_t*);
std::uint8_t APS5_VABI u_getCombiningClass_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_charDirection_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_isMirrored_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_charMirror_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_getBidiPairedBracket_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_foldCase_nid_postfix(std::int32_t, std::uint32_t);
std::int32_t APS5_VABI u_getPropertyEnum_nid_postfix(const char*);
const char* APS5_VABI u_getPropertyName_nid_postfix(std::int32_t, std::int32_t);
std::int32_t APS5_VABI u_getPropertyValueEnum_nid_postfix(std::int32_t, const char*);
const char* APS5_VABI u_getPropertyValueName_nid_postfix(std::int32_t, std::int32_t, std::int32_t);
std::int32_t APS5_VABI u_getIntPropertyMinValue_nid_postfix(std::int32_t);
std::int32_t APS5_VABI u_getIntPropertyMaxValue_nid_postfix(std::int32_t);
std::int8_t APS5_VABI u_hasBinaryProperty_nid_postfix(std::int32_t, std::int32_t);
std::int32_t APS5_VABI u_getIntPropertyValue_nid_postfix(std::int32_t, std::int32_t);
}
