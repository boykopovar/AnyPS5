#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UResourceBundle;

extern "C" {

UResourceBundle* APS5_VABI ures_open(const char*, const char*, std::int32_t*);
UResourceBundle* APS5_VABI ures_openDirect_nid_postfix(const char*, const char*, std::int32_t*);
void APS5_VABI ures_close(UResourceBundle*);
UResourceBundle* APS5_VABI ures_getByKey(const UResourceBundle*, const char*, UResourceBundle*, std::int32_t*);
UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle*, std::int32_t, UResourceBundle*, std::int32_t*);
std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle*);
std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle*);
const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle*);
const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*);
const char16_t* APS5_VABI ures_getStringByKey(const UResourceBundle*, const char*, std::int32_t*, std::int32_t*);
const char16_t* APS5_VABI ures_getStringByIndex_nid_postfix(const UResourceBundle*, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle*, std::int32_t*);
const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*);
const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle*, std::int32_t*, std::int32_t*);

}
