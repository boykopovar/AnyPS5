#include "prx/libSceIcu/NumberFormatBackend.hpp"
#include "unicode/unum.h"
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace IcuNumberBackend {

static_assert(sizeof(std::int32_t) == 4 && sizeof(UErrorCode) == 4);
static_assert(sizeof(UNumberFormatStyle) == 4 && sizeof(UNumberFormatSymbol) == 4);
static_assert(sizeof(UNumberFormatAttribute) == 4 && sizeof(UNumberFormatTextAttribute) == 4);
static_assert(sizeof(char16_t) == 2 && std::is_same_v<UChar, char16_t>);
static_assert(U_PARSE_CONTEXT_LEN == 16);
static_assert(U_DECIMAL_NUMBER_SYNTAX_ERROR == 0x10110 && U_FORMAT_INEXACT_ERROR == 0x10111);
static_assert(std::is_trivially_copyable_v<Field> && std::is_trivially_copyable_v<UFieldPosition>);
static_assert(sizeof(Field) == 12 && sizeof(UFieldPosition) == 12);
static_assert(alignof(Field) == 4 && alignof(UFieldPosition) == 4);
static_assert(offsetof(Field, field) == 0 && offsetof(UFieldPosition, field) == 0);
static_assert(offsetof(Field, begin) == 4 && offsetof(UFieldPosition, beginIndex) == 4);
static_assert(offsetof(Field, end) == 8 && offsetof(UFieldPosition, endIndex) == 8);
static_assert(std::is_trivially_copyable_v<Parse> && std::is_trivially_copyable_v<UParseError>);
static_assert(sizeof(Parse) == 72 && sizeof(UParseError) == 72);
static_assert(alignof(Parse) == 4 && alignof(UParseError) == 4);
static_assert(offsetof(Parse, line) == 0 && offsetof(UParseError, line) == 0);
static_assert(offsetof(Parse, offset) == 4 && offsetof(UParseError, offset) == 4);
static_assert(offsetof(Parse, before) == 8 && offsetof(UParseError, preContext) == 8);
static_assert(offsetof(Parse, after) == 40 && offsetof(UParseError, postContext) == 40);

void* Open_nid_no_patch(std::int32_t style, const char16_t* pattern, std::int32_t length,
                        const char* locale, Parse* parse, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UParseError record;
    if (parse != nullptr) std::memcpy(&record, parse, sizeof(record));
    auto* result = unum_open(static_cast<UNumberFormatStyle>(style), pattern, length, locale,
                            parse != nullptr ? &record : nullptr, &error);
    if (parse != nullptr) std::memcpy(parse, &record, sizeof(record));
    *status = error;
    return result;
}

void* Clone_nid_no_patch(const void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = unum_clone(static_cast<const UNumberFormat*>(handle), &error);
    *status = error;
    return result;
}

void Close_nid_no_patch(void* handle) {
    unum_close(static_cast<UNumberFormat*>(handle));
}

std::int32_t Double_nid_no_patch(const void* handle, double number, char16_t* output,
                                std::int32_t capacity, Field* field, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UFieldPosition record;
    if (field != nullptr) std::memcpy(&record, field, sizeof(record));
    const auto result = unum_formatDouble(static_cast<const UNumberFormat*>(handle), number, output,
                                          capacity, field != nullptr ? &record : nullptr, &error);
    if (field != nullptr) std::memcpy(field, &record, sizeof(record));
    *status = error;
    return result;
}

std::int32_t Decimal_nid_no_patch(const void* handle, const char* number, std::int32_t length,
                                 char16_t* output, std::int32_t capacity, Field* field, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UFieldPosition record;
    if (field != nullptr) std::memcpy(&record, field, sizeof(record));
    const auto result = unum_formatDecimal(static_cast<const UNumberFormat*>(handle), number, length,
                                           output, capacity, field != nullptr ? &record : nullptr, &error);
    if (field != nullptr) std::memcpy(field, &record, sizeof(record));
    *status = error;
    return result;
}

std::int32_t Symbol_nid_no_patch(const void* handle, std::int32_t symbol, char16_t* output,
                                std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = unum_getSymbol(static_cast<const UNumberFormat*>(handle),
                                       static_cast<UNumberFormatSymbol>(symbol), output, capacity, &error);
    *status = error;
    return result;
}

std::int32_t Text_nid_no_patch(const void* handle, std::int32_t attribute, char16_t* output,
                              std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = unum_getTextAttribute(static_cast<const UNumberFormat*>(handle),
                                              static_cast<UNumberFormatTextAttribute>(attribute), output, capacity, &error);
    *status = error;
    return result;
}

void SetAttribute_nid_no_patch(void* handle, std::int32_t attribute, std::int32_t value) {
    unum_setAttribute(static_cast<UNumberFormat*>(handle), static_cast<UNumberFormatAttribute>(attribute), value);
}

void SetText_nid_no_patch(void* handle, std::int32_t attribute, const char16_t* value,
                         std::int32_t length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    unum_setTextAttribute(static_cast<UNumberFormat*>(handle), static_cast<UNumberFormatTextAttribute>(attribute),
                          value, length, &error);
    *status = error;
}

} // namespace IcuNumberBackend
