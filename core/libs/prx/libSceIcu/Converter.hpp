#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

extern "C" {

UConverter* APS5_VABI ucnv_open_nid_postfix(const char*, std::int32_t*);
void APS5_VABI ucnv_close_nid_postfix(UConverter*);
const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter*, std::int32_t*);
void APS5_VABI ucnv_reset_nid_postfix(UConverter*);
void APS5_VABI ucnv_resetToUnicode_nid_postfix(UConverter*);
void APS5_VABI ucnv_resetFromUnicode_nid_postfix(UConverter*);
void APS5_VABI ucnv_setFallback_nid_postfix(UConverter*, std::int8_t);
void APS5_VABI ucnv_setSubstChars_nid_postfix(UConverter*, const char*, std::int8_t, std::int32_t*);
std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(UConverter*, char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter*, char*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);
void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter*, char16_t**, const char16_t*, const char**, const char*, std::int32_t*, std::int8_t, std::int32_t*);
void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter*, char**, const char*, const char16_t**, const char16_t*, std::int32_t*, std::int8_t, std::int32_t*);
void APS5_VABI ucnv_convertEx_nid_postfix(UConverter*, UConverter*, char**, const char*, const char**, const char*,
                                           char16_t*, char16_t**, char16_t**, const char16_t*, std::int8_t, std::int8_t, std::int32_t*);

}
