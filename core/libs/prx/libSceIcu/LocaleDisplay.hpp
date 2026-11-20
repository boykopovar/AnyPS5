#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uloc_getDisplayName_nid_postfix(const char* localeID, const char* inLocaleID,
    char16_t* result, std::int32_t maxResultSize, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayLanguage_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* language, std::int32_t languageCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayScript_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* script, std::int32_t scriptCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayCountry_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* country, std::int32_t countryCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayVariant_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* variant, std::int32_t variantCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayKeyword_nid_postfix(const char* keyword, const char* displayLocale,
    char16_t* destination, std::int32_t destinationCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getDisplayKeywordValue_nid_postfix(const char* locale, const char* keyword,
    const char* displayLocale, char16_t* destination, std::int32_t destinationCapacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getLineOrientation_nid_postfix(const char* localeID, std::int32_t* status);
}
