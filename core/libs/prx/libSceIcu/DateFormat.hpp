#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

using UDateFormat = void*;
using UCalendar = void*;
struct UFieldPosition;

extern "C" {

void APS5_VABI udat_close_nid_postfix(UDateFormat*);
std::int32_t APS5_VABI udat_countSymbols_nid_postfix(const UDateFormat* fmt, std::int32_t type);
std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat* format, double dateToFormat, char16_t* result, std::int32_t resultLength, UFieldPosition* position, std::int32_t* status);
std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status);
const UCalendar* APS5_VABI udat_getCalendar_nid_postfix(const UDateFormat* fmt);
std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t type, std::int32_t index, char16_t* result,
                                                   std::int32_t resultLength, std::int32_t* status);
UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* fmt, std::int8_t localized, char16_t* result, std::int32_t resultLength, std::int32_t* status);

}
