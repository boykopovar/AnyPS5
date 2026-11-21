#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

namespace Icu {
struct ConverterFromUnicodeArgs;
struct ConverterToUnicodeArgs;
using ConverterFromUCallback = void (APS5_VABI*)(const void* context, ConverterFromUnicodeArgs* arguments,
                                                  const char16_t* codeUnits, std::int32_t length,
                                                  std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
using ConverterToUCallback = void (APS5_VABI*)(const void* context, ConverterToUnicodeArgs* arguments,
                                               const char* codeUnits, std::int32_t length, std::int32_t reason,
                                               std::int32_t* error);
}

extern "C" {
void APS5_VABI ucnv_setFromUCallBack(UConverter* converter, Icu::ConverterFromUCallback newAction, const void* newContext,
                                     Icu::ConverterFromUCallback* oldAction, const void** oldContext,
                                     std::int32_t* error);
void APS5_VABI ucnv_setToUCallBack(UConverter* converter, Icu::ConverterToUCallback newAction, const void* newContext,
                                   Icu::ConverterToUCallback* oldAction, const void** oldContext, std::int32_t* error);
void APS5_VABI UCNV_FROM_U_CALLBACK_STOP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_STOP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
void APS5_VABI UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error);
void APS5_VABI ucnv_cbFromUWriteSub_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* error);
}
