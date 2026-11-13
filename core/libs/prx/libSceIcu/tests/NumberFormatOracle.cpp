#include "prx/libSceIcu/tests/NumberFormatOracle.hpp"
#include "unicode/unum.h"
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace NumberFormatOracle {

static_assert(sizeof(UErrorCode) == 4 && std::is_same_v<UChar, char16_t>);
static_assert(sizeof(Field) == 12 && sizeof(Field) == sizeof(UFieldPosition));
static_assert(alignof(Field) == alignof(UFieldPosition));
static_assert(offsetof(Field, field) == offsetof(UFieldPosition, field));
static_assert(offsetof(Field, begin) == offsetof(UFieldPosition, beginIndex));
static_assert(offsetof(Field, end) == offsetof(UFieldPosition, endIndex));
static_assert(sizeof(Parse) == 72 && sizeof(Parse) == sizeof(UParseError));
static_assert(alignof(Parse) == alignof(UParseError));
static_assert(offsetof(Parse, line) == offsetof(UParseError, line));
static_assert(offsetof(Parse, offset) == offsetof(UParseError, offset));
static_assert(offsetof(Parse, before) == offsetof(UParseError, preContext));
static_assert(offsetof(Parse, after) == offsetof(UParseError, postContext));

void* Open(std::int32_t style, const char16_t* pattern, std::int32_t length, const char* locale,
           Parse* parse, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UParseError record;
    if (parse) std::memcpy(&record, parse, sizeof(record));
    auto* result = unum_open(static_cast<UNumberFormatStyle>(style), pattern, length, locale, parse ? &record : nullptr, &error);
    if (parse) std::memcpy(parse, &record, sizeof(record));
    *status = error;
    return result;
}

void* Clone(const void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = unum_clone(static_cast<const UNumberFormat*>(handle), &error);
    *status = error;
    return result;
}

void Close(void* handle) { unum_close(static_cast<UNumberFormat*>(handle)); }

std::int32_t Double(const void* handle, double value, char16_t* output, std::int32_t capacity,
                    Field* field, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UFieldPosition record;
    if (field) std::memcpy(&record, field, sizeof(record));
    const auto count = unum_formatDouble(static_cast<const UNumberFormat*>(handle), value, output, capacity,
        field ? &record : nullptr, &error);
    if (field) std::memcpy(field, &record, sizeof(record));
    *status = error;
    return count;
}

std::int32_t Decimal(const void* handle, const char* value, std::int32_t length, char16_t* output,
                     std::int32_t capacity, Field* field, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    UFieldPosition record;
    if (field) std::memcpy(&record, field, sizeof(record));
    const auto count = unum_formatDecimal(static_cast<const UNumberFormat*>(handle), value, length, output,
        capacity, field ? &record : nullptr, &error);
    if (field) std::memcpy(field, &record, sizeof(record));
    *status = error;
    return count;
}

std::int32_t Symbol(const void* handle, std::int32_t symbol, char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto count = unum_getSymbol(static_cast<const UNumberFormat*>(handle), static_cast<UNumberFormatSymbol>(symbol), output, capacity, &error);
    *status = error;
    return count;
}

std::int32_t Text(const void* handle, std::int32_t attribute, char16_t* output, std::int32_t capacity, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto count = unum_getTextAttribute(static_cast<const UNumberFormat*>(handle), static_cast<UNumberFormatTextAttribute>(attribute), output, capacity, &error);
    *status = error;
    return count;
}

void SetAttribute(void* handle, std::int32_t attribute, std::int32_t value) {
    unum_setAttribute(static_cast<UNumberFormat*>(handle), static_cast<UNumberFormatAttribute>(attribute), value);
}

void SetText(void* handle, std::int32_t attribute, const char16_t* value, std::int32_t length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    unum_setTextAttribute(static_cast<UNumberFormat*>(handle), static_cast<UNumberFormatTextAttribute>(attribute), value, length, &error);
    *status = error;
}

}
