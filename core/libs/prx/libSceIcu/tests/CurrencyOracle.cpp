#include <string_view>
#include "prx/libSceIcu/tests/CurrencyOracle.hpp"
#include "unicode/ucurr.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include <stdexcept>

namespace CurrencyOracle {

void SetDefault(const char* locale) {
    UErrorCode status = U_ZERO_ERROR;
    uloc_setDefault(locale, &status);
    if (U_FAILURE(status)) throw std::runtime_error("native currency oracle default locale failed");
}

ScalarResult Scalar(unsigned operation, const char16_t* code, const char* locale, std::int32_t usage, double from, double to, std::int32_t error) {
    auto status = static_cast<UErrorCode>(error);
    double result = 0;
    switch (operation) {
        case 0: result = ucurr_getDefaultFractionDigits(code, &status); break;
        case 1: result = ucurr_getDefaultFractionDigitsForUsage(code, static_cast<UCurrencyUsage>(usage), &status); break;
        case 2: result = ucurr_getRoundingIncrement(code, &status); break;
        case 3: result = ucurr_getRoundingIncrementForUsage(code, static_cast<UCurrencyUsage>(usage), &status); break;
        case 4: result = ucurr_getNumericCode(code); break;
        case 5: result = ucurr_isAvailable(code, from, to, &status); break;
        case 6: result = ucurr_countCurrencies(locale, from, &status); break;
        default: throw std::runtime_error("unknown scalar oracle operation");
    }
    return {result, status};
}

CodeResult Code(const char* locale, double date, std::int32_t index, std::int32_t capacity, std::int32_t error, bool historical) {
    CodeResult result;
    result.text.fill(0x5a5a);
    auto status = static_cast<UErrorCode>(error);
    result.length = historical ? ucurr_forLocaleAndDate(locale, date, index, result.text.data(), capacity, &status)
        : ucurr_forLocale(locale, result.text.data(), capacity, &status);
    result.status = status;
    return result;
}

NameResult Name(const char16_t* code, const char* locale, std::int32_t style, const char* plural, std::int32_t error, bool pluralName) {
    NameResult result{{}, false, false, -99, error, 73};
    auto status = static_cast<UErrorCode>(error);
    const auto* text = pluralName ? ucurr_getPluralName(code, locale, &result.choice, plural, &result.length, &status)
        : ucurr_getName(code, locale, static_cast<UCurrNameStyle>(style), &result.choice, &result.length, &status);
    result.status = status;
    result.null = text == nullptr;
    result.alias = text && text == code;
    if (text && result.length >= 0) result.text.assign(text, static_cast<std::size_t>(result.length));
    return result;
}

EnumerationResult Enumeration(std::uint32_t type, const char* locale, bool keywords, std::int32_t error, bool commonlyUsed) {
    EnumerationResult result;
    auto status = static_cast<UErrorCode>(error);
    auto* enumeration = keywords ? ucurr_getKeywordValuesForLocale("currency", locale, commonlyUsed, &status) : ucurr_openISOCurrencies(type, &status);
    result.status = status;
    if (!enumeration) throw std::runtime_error("native currency oracle enumeration failed");
    try {
        status = U_ZERO_ERROR;
        result.count = uenum_count(enumeration, &status);
        for (;;) {
            std::int32_t length = -99;
            const auto* next = uenum_next(enumeration, &length, &status);
            if (!next) { result.endLength = length; break; }
            result.values.emplace_back(next, static_cast<std::size_t>(length));
        }
        uenum_reset(enumeration, &status);
        for (;;) {
            std::int32_t length = -99;
            const auto* next = uenum_unext(enumeration, &length, &status);
            if (!next) { result.unicodeEndLength = length; break; }
            result.unicode.emplace_back(next, static_cast<std::size_t>(length));
        }
        if (U_FAILURE(status)) throw std::runtime_error("native currency oracle enumeration iteration failed");
        uenum_close(enumeration);
        return result;
    } catch (...) {
        uenum_close(enumeration);
        throw;
    }
}

bool EnumerationWriteBounds(std::int32_t* readOnly, std::int32_t* ignoredLength) {
    UErrorCode status = U_ZERO_ERROR;
    auto* enumeration = ucurr_getKeywordValuesForLocale("currency", "en_US", true, &status);
    if (!enumeration || U_FAILURE(status)) throw std::runtime_error("native keyword guard setup failed");
    std::int32_t length = -99;
    const auto* first = uenum_next(enumeration, &length, reinterpret_cast<UErrorCode*>(readOnly));
    bool matched = first && std::string_view(first, length) == "USD" && *readOnly == 0;
    while (uenum_next(enumeration, &length, &status)) {}
    matched = matched && !uenum_next(enumeration, ignoredLength, &status) && status == U_ZERO_ERROR;
    length = -99;
    matched = matched && !uenum_unext(enumeration, &length, &status) && length == 0 && status == U_ZERO_ERROR;
    uenum_close(enumeration);
    return matched;
}

}
