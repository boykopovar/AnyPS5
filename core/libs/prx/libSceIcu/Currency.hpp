#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UEnumeration;
using UCurrRegistryKey = const void*;

extern "C" {
std::int32_t APS5_VABI ucurr_forLocale_nid_postfix(const char*, char16_t*, std::int32_t, std::int32_t*);
const char16_t* APS5_VABI ucurr_getName_nid_postfix(const char16_t*, const char*, std::int32_t, std::int8_t*, std::int32_t*, std::int32_t*);
const char16_t* APS5_VABI ucurr_getPluralName_nid_postfix(const char16_t*, const char*, std::int8_t*, const char*, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI ucurr_getDefaultFractionDigits_nid_postfix(const char16_t*, std::int32_t*);
std::int32_t APS5_VABI ucurr_getDefaultFractionDigitsForUsage_nid_postfix(const char16_t*, std::int32_t, std::int32_t*);
double APS5_VABI ucurr_getRoundingIncrement_nid_postfix(const char16_t*, std::int32_t*);
double APS5_VABI ucurr_getRoundingIncrementForUsage_nid_postfix(const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ucurr_getNumericCode_nid_postfix(const char16_t*);
std::int8_t APS5_VABI ucurr_isAvailable_nid_postfix(const char16_t*, double, double, std::int32_t*);
std::int32_t APS5_VABI ucurr_countCurrencies_nid_postfix(const char*, double, std::int32_t*);
std::int32_t APS5_VABI ucurr_forLocaleAndDate_nid_postfix(const char*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
UEnumeration* APS5_VABI ucurr_openISOCurrencies_nid_postfix(std::uint32_t, std::int32_t*);
UEnumeration* APS5_VABI ucurr_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t*);
UCurrRegistryKey APS5_VABI ucurr_register_nid_postfix(const char16_t*, const char*, std::int32_t*);
std::int8_t APS5_VABI ucurr_unregister_nid_postfix(UCurrRegistryKey, std::int32_t*);
}
