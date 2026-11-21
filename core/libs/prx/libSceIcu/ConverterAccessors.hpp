#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

extern "C" {
std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(UConverter* converter, const char** source,
    const char* sourceLimit, std::int32_t* status);
void APS5_VABI ucnv_getFromUCallBack_nid_postfix(const UConverter* converter, void** action, void** context);
void APS5_VABI ucnv_getToUCallBack_nid_postfix(const UConverter* converter, void** action, void** context);
extern "C" std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination, std::int32_t capacity, const char16_t* source, std::int32_t length, std::int32_t* status);
void APS5_VABI ucnv_getInvalidChars_nid_postfix(const UConverter* converter, char* errorBytes,
    std::int8_t* errorLength, std::int32_t* status);
void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const UConverter* converter, char16_t* errorChars,
    std::int8_t* errorLength, std::int32_t* status);
void APS5_VABI ucnv_setSubstString_nid_postfix(UConverter* converter, const char16_t* substitution,
    std::int32_t length, std::int32_t* status);
void APS5_VABI ucnv_getStarters_nid_postfix(const UConverter* converter, std::int8_t* starters, std::int32_t* status);
UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status);
void APS5_VABI ucnv_close_nid_postfix(UConverter* converter);
const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status);
void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter);
void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int32_t usesFallback);
std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const UConverter* converter);
std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const UConverter* converter);
std::int32_t APS5_VABI ucnv_getType_nid_postfix(const UConverter* converter);
std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int8_t APS5_VABI ucnv_getPlatform_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const UConverter* converter);
std::int32_t APS5_VABI ucnv_isFixedWidth_nid_postfix(UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_usesFallback_nid_postfix(const UConverter* converter);
std::int32_t APS5_VABI ucnv_countAvailable_nid_postfix();
const char* APS5_VABI ucnv_getAvailableName_nid_postfix(std::int32_t number);
std::int32_t APS5_VABI ucnv_countStandards_nid_postfix();
const char* APS5_VABI ucnv_getStandard_nid_postfix(std::int32_t number, std::int32_t* status);
const char* APS5_VABI ucnv_getStandardName_nid_postfix(const char* name, const char* standard, std::int32_t* status);
std::int32_t APS5_VABI ucnv_countAliases_nid_postfix(const char* alias, std::int32_t* status);
const char* APS5_VABI ucnv_getAlias_nid_postfix(const char* alias, std::uint16_t number, std::int32_t* status);
std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status);
std::int32_t APS5_VABI ucnv_flushCache_nid_postfix();
}
