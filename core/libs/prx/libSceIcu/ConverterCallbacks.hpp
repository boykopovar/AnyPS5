#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct ConverterFromUnicodeArgs;
struct ConverterToUnicodeArgs;
}

extern "C" {
void APS5_VABI UCNV_FROM_U_CALLBACK_STOP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_STOP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
}
