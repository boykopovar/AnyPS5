#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct Converter;
} // namespace Icu

extern "C" {
Icu::Converter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* error);
Icu::Converter* APS5_VABI ucnv_openU_nid_postfix(const char16_t* name, std::int32_t* error);
void APS5_VABI ucnv_close_nid_postfix(Icu::Converter* converter);
const char* APS5_VABI ucnv_getName_nid_postfix(const Icu::Converter* converter, std::int32_t* error);
void APS5_VABI ucnv_reset_nid_postfix(Icu::Converter* converter);
void APS5_VABI ucnv_resetToUnicode_nid_postfix(Icu::Converter* converter);
void APS5_VABI ucnv_resetFromUnicode_nid_postfix(Icu::Converter* converter);
std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const Icu::Converter* converter);
std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const Icu::Converter* converter);
std::int32_t APS5_VABI ucnv_getType_nid_postfix(const Icu::Converter* converter);
std::int8_t APS5_VABI ucnv_isFixedWidth_nid_postfix(Icu::Converter* converter, std::int32_t* error);
std::int8_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const Icu::Converter* converter);
std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const Icu::Converter* converter, std::int32_t* error);
std::int32_t APS5_VABI ucnv_getPlatform_nid_postfix(const Icu::Converter* converter, std::int32_t* error);
void APS5_VABI ucnv_getStarters_nid_postfix(const Icu::Converter* converter, std::int8_t* starters, std::int32_t* error);
void APS5_VABI ucnv_getSubstChars_nid_postfix(const Icu::Converter* converter, char* bytes, std::int8_t* length, std::int32_t* error);
void APS5_VABI ucnv_setSubstChars_nid_postfix(Icu::Converter* converter, const char* bytes, std::int8_t length, std::int32_t* error);
void APS5_VABI ucnv_setSubstString_nid_postfix(Icu::Converter* converter, const char16_t* text, std::int32_t length, std::int32_t* error);
void APS5_VABI ucnv_getInvalidChars_nid_postfix(const Icu::Converter* converter, char* bytes, std::int8_t* length, std::int32_t* error);
void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const Icu::Converter* converter, char16_t* units, std::int8_t* length,
                                                 std::int32_t* error);
void APS5_VABI ucnv_setFallback_nid_postfix(Icu::Converter* converter, std::int8_t usesFallback);
std::int8_t APS5_VABI ucnv_usesFallback_nid_postfix(const Icu::Converter* converter);
std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const Icu::Converter* converter, std::int32_t* error);
std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const Icu::Converter* converter, std::int32_t* error);
void APS5_VABI ucnv_fromUnicode_nid_postfix(Icu::Converter* converter, char** target, const char* targetLimit, const char16_t** source,
                                            const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* error);
void APS5_VABI ucnv_toUnicode_nid_postfix(Icu::Converter* converter, char16_t** target, const char16_t* targetLimit, const char** source,
                                          const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* error);
std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(Icu::Converter* converter, char* destination, std::int32_t capacity, const char16_t* source,
                                                   std::int32_t length, std::int32_t* error);
std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(Icu::Converter* converter, char16_t* destination, std::int32_t capacity, const char* source,
                                                 std::int32_t length, std::int32_t* error);
std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(Icu::Converter* converter, const char** source, const char* sourceLimit, std::int32_t* error);
void APS5_VABI ucnv_convertEx_nid_postfix(Icu::Converter* targetConverter, Icu::Converter* sourceConverter, char** target,
                                          const char* targetLimit, const char** source, const char* sourceLimit, char16_t* pivotStart,
                                          char16_t** pivotSource, char16_t** pivotTarget, const char16_t* pivotLimit, std::int8_t reset,
                                          std::int8_t flush, std::int32_t* error);
std::int32_t APS5_VABI ucnv_convert_nid_postfix(const char* targetName, const char* sourceName, char* target, std::int32_t capacity,
                                                const char* source, std::int32_t length, std::int32_t* error);
std::int32_t APS5_VABI ucnv_toAlgorithmic_nid_postfix(std::int32_t type, Icu::Converter* converter, char* target, std::int32_t capacity,
                                                      const char* source, std::int32_t length, std::int32_t* error);
std::int32_t APS5_VABI ucnv_fromAlgorithmic_nid_postfix(Icu::Converter* converter, std::int32_t type, char* target, std::int32_t capacity,
                                                        const char* source, std::int32_t length, std::int32_t* error);
}
