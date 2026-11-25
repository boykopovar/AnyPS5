#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct ULocale;
struct UEnumeration;

extern "C" {
ULocale* APS5_VABI ulocale_openForLocaleID_nid_postfix(const char*, std::int32_t, std::int32_t*);
void APS5_VABI ulocale_close_nid_postfix(ULocale*);
const char* APS5_VABI ulocale_getLocaleID_nid_postfix(const ULocale*);
bool APS5_VABI ulocale_isBogus_nid_postfix(const ULocale*);
ULocale* APS5_VABI ulocale_openForLanguageTag_nid_postfix(const char*, std::int32_t, std::int32_t*);
const char* APS5_VABI ulocale_getBaseName_nid_postfix(const ULocale*);
std::int32_t APS5_VABI ulocale_getKeywordValue_nid_postfix(const ULocale*, const char*, std::int32_t, char*,
                                                           std::int32_t, std::int32_t*);
UEnumeration* APS5_VABI ulocale_getKeywords_nid_postfix(const ULocale*, std::int32_t*);
const char* APS5_VABI ulocale_getLanguage_nid_postfix(const ULocale*);
const char* APS5_VABI ulocale_getRegion_nid_postfix(const ULocale*);
const char* APS5_VABI ulocale_getScript_nid_postfix(const ULocale*);
std::int32_t APS5_VABI ulocale_getUnicodeKeywordValue_nid_postfix(const ULocale*, const char*, std::int32_t, char*,
                                                                  std::int32_t, std::int32_t*);
UEnumeration* APS5_VABI ulocale_getUnicodeKeywords_nid_postfix(const ULocale*, std::int32_t*);
const char* APS5_VABI ulocale_getVariant_nid_postfix(const ULocale*);
}
