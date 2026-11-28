#include "prx/libSceIcu/CollatorHost.hpp"
#include "unicode/ucol.h"

namespace Icu::CollatorHost {

namespace {

constexpr std::int32_t LowestValue = UCOL_DEFAULT;
constexpr std::int32_t HighestValue = UCOL_UPPER_FIRST;

bool KnownAttribute(std::int32_t attribute) {
    return attribute >= UCOL_FRENCH_COLLATION && attribute <= UCOL_NUMERIC_COLLATION;
}

bool KnownValue(std::int32_t value) {
    return value >= LowestValue && value <= HighestValue;
}

void Reject(std::int32_t& status) {
    if (status <= U_ZERO_ERROR) status = U_ILLEGAL_ARGUMENT_ERROR;
}

} // namespace

void* Open(const char* locale, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    UCollator* collator = ucol_open(locale, &code);
    status = code;
    return collator;
}

void Close(void* collator) {
    ucol_close(static_cast<UCollator*>(collator));
}

std::int32_t GetAttribute(const void* collator, std::int32_t attribute, std::int32_t& status) {
    if (status > U_ZERO_ERROR) return UCOL_DEFAULT;
    if (!KnownAttribute(attribute)) {
        Reject(status);
        return UCOL_DEFAULT;
    }
    auto code = static_cast<UErrorCode>(status);
    const auto value = ucol_getAttribute(static_cast<const UCollator*>(collator), static_cast<UColAttribute>(attribute), &code);
    status = code;
    return value;
}

void SetAttribute(void* collator, std::int32_t attribute, std::int32_t value, std::int32_t& status) {
    if (status > U_ZERO_ERROR) return;
    if (!KnownAttribute(attribute) || !KnownValue(value)) {
        Reject(status);
        return;
    }
    auto code = static_cast<UErrorCode>(status);
    ucol_setAttribute(static_cast<UCollator*>(collator), static_cast<UColAttribute>(attribute),
                      static_cast<UColAttributeValue>(value), &code);
    status = code;
}

std::int32_t Compare(const void* collator, const char16_t* source, std::int32_t sourceLength, const char16_t* target,
                     std::int32_t targetLength) {
    return ucol_strcoll(static_cast<const UCollator*>(collator), source, sourceLength, target, targetLength);
}

std::int32_t CompareUtf8(const void* collator, const char* source, std::int32_t sourceLength, const char* target,
                         std::int32_t targetLength, std::int32_t& status) {
    auto code = static_cast<UErrorCode>(status);
    const auto result = ucol_strcollUTF8(static_cast<const UCollator*>(collator), source, sourceLength, target, targetLength, &code);
    status = code;
    return result;
}

} // namespace Icu::CollatorHost
