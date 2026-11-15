#include "prx/libSceIcu/CurrencyBackend.hpp"
#include "unicode/ucurr.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include "ulocimp.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

static_assert(sizeof(UBool) == 1);
static_assert(sizeof(UErrorCode) == 4);
static_assert(sizeof(UDate) == 8);
static_assert(std::is_same_v<UChar, char16_t>);

namespace IcuCurrency {

void SetDefault_nid_no_patch(const char* locale, std::int32_t& error) {
    if (error > 0 || std::strcmp(uloc_getDefault(), locale) == 0) return;
    auto status = static_cast<UErrorCode>(error);
    uloc_setDefault(locale, &status);
    error = status;
}

bool ValidNameLocale_nid_no_patch(const char* locale) {
    UErrorCode status = U_ZERO_ERROR;
    const auto name = ulocimp_getName(locale, status);
    return U_SUCCESS(status);
}

CodeResult ForLocale_nid_no_patch(const char* locale, std::int32_t capacity, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    CodeResult result;
    result.length = ucurr_forLocale(locale, result.text.data(), std::min(capacity, 4), &status);
    if (U_SUCCESS(status) && result.length >= 0 && result.length < 4 && capacity > result.length) result.written = result.length + 1;
    error = status;
    return result;
}

CodeResult ForLocaleAndDate_nid_no_patch(const char* locale, double date, std::int32_t index, std::int32_t capacity, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    CodeResult result;
    result.length = ucurr_forLocaleAndDate(locale, date, index, result.text.data(), std::min(capacity, 4), &status);
    if (U_SUCCESS(status) && result.length > 0 && result.length < 4 && capacity > result.length) result.written = result.length + 1;
    error = status;
    return result;
}

NameResult Name_nid_no_patch(const char16_t* currency, const char* locale, std::int32_t style, const char* plural, bool pluralName, bool choice, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    NameResult result;
    auto* choiceOutput = choice ? &result.choice : nullptr;
    result.text = pluralName ? ucurr_getPluralName(currency, locale, choiceOutput, plural, &result.length, &status)
        : ucurr_getName(currency, locale, static_cast<UCurrNameStyle>(style), choiceOutput, &result.length, &status);
    result.inputAlias = result.text && result.text == currency;
    error = status;
    return result;
}

std::int32_t FractionDigits_nid_no_patch(const char16_t* currency, std::int32_t usage, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = ucurr_getDefaultFractionDigitsForUsage(currency, static_cast<UCurrencyUsage>(usage), &status);
    error = status;
    return result;
}

double RoundingIncrement_nid_no_patch(const char16_t* currency, std::int32_t usage, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    double result = 0;
    if (usage == UCURR_USAGE_STANDARD || usage == UCURR_USAGE_CASH) {
        result = ucurr_getRoundingIncrementForUsage(currency, static_cast<UCurrencyUsage>(usage), &status);
    } else {
        ucurr_getDefaultFractionDigits(currency, &status);
        if (U_SUCCESS(status)) status = U_UNSUPPORTED_ERROR;
    }
    error = status;
    return result;
}

std::int32_t NumericCode_nid_no_patch(const char16_t* currency) {
    return ucurr_getNumericCode(currency);
}

std::int8_t IsAvailable_nid_no_patch(const char16_t* currency, double from, double to, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = ucurr_isAvailable(currency, from, to, &status);
    error = status;
    return result;
}

std::int32_t CountCurrencies_nid_no_patch(const char* locale, double date, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = ucurr_countCurrencies(locale, date, &status);
    error = status;
    return result;
}

void* OpenISO_nid_no_patch(std::uint32_t type, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    auto* result = ucurr_openISOCurrencies(type, &status);
    error = status;
    return result;
}

void* OpenKeywords_nid_no_patch(const char* locale, std::int8_t commonlyUsed, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    auto* result = ucurr_getKeywordValuesForLocale(nullptr, locale, commonlyUsed, &status);
    error = status;
    return result;
}

const void* Register_nid_no_patch(const char16_t* currency, const char* locale, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = ucurr_register(currency, locale, &status);
    error = status;
    return result;
}

std::int8_t Unregister_nid_no_patch(const void* key, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = ucurr_unregister(key, &status);
    error = status;
    return result;
}

void CloseEnumeration_nid_no_patch(void* enumeration) {
    uenum_close(static_cast<UEnumeration*>(enumeration));
}

std::int32_t CountEnumeration_nid_no_patch(void* enumeration, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    const auto result = uenum_count(static_cast<UEnumeration*>(enumeration), &status);
    error = status;
    return result;
}

NextResult NextEnumeration_nid_no_patch(void* enumeration, bool unicode, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    NextResult result;
    if (unicode) result.text = uenum_unext(static_cast<UEnumeration*>(enumeration), &result.length, &status);
    else result.text = uenum_next(static_cast<UEnumeration*>(enumeration), &result.length, &status);
    error = status;
    return result;
}

void ResetEnumeration_nid_no_patch(void* enumeration, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    uenum_reset(static_cast<UEnumeration*>(enumeration), &status);
    error = status;
}

}
