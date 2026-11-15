// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucurr.cpp
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2002-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "prx/libSceIcu/CurrencyBackend.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucurr.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "cstring.h"
#include "uenumimp.h"
#include "ulist.h"
#include "ulocimp.h"
#include "uresimp.h"
#include "ureslocs.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <type_traits>
#include <utility>

static_assert(sizeof(UBool) == 1);
static_assert(sizeof(UErrorCode) == 4);
static_assert(sizeof(UDate) == 8);
static_assert(std::is_same_v<UChar, char16_t>);

namespace IcuCurrency {

namespace {

struct ResourceSet {
    std::array<UResourceBundle*, 5> items{};
    void Close() {
        for (auto iterator = items.rbegin(); iterator != items.rend(); ++iterator) ures_close(std::exchange(*iterator, nullptr));
    }
};

struct DateBound {
    double value;
    bool present;
};

DateBound ReadDate(ResourceSet& resources, const char* key, double absent, UErrorCode& status) {
    if (U_FAILURE(status)) return {absent, false};
    UErrorCode fieldStatus = U_ZERO_ERROR;
    auto*& field = resources.items[4];
    field = ures_getByKey(resources.items[3], key, nullptr, &fieldStatus);
    DateBound result{absent, false};
    if (fieldStatus != U_MISSING_RESOURCE_ERROR) {
        if (U_SUCCESS(fieldStatus)) {
            std::int32_t length = 0;
            const auto* parts = ures_getIntVector(field, &length, &fieldStatus);
            if (U_SUCCESS(fieldStatus) && parts && length == 2) {
                const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[0])) << 32)
                    | static_cast<std::uint32_t>(parts[1]);
                result = {static_cast<double>(std::bit_cast<std::int64_t>(bits)), true};
            } else if (U_SUCCESS(fieldStatus)) {
                fieldStatus = U_INVALID_FORMAT_ERROR;
            }
        }
        if (U_FAILURE(fieldStatus)) status = fieldStatus;
    }
    ures_close(std::exchange(field, nullptr));
    return result;
}

struct HistoricalResult {
    std::int32_t count = 0;
    CodeResult selected;
};

void NameWarning(UErrorCode local, UErrorCode& status) {
    if (U_SUCCESS(local) && (local == U_USING_DEFAULT_WARNING || (local == U_USING_FALLBACK_WARNING && status != U_USING_DEFAULT_WARNING))) status = local;
}

NameResult BasicName(const char* locale, std::int32_t style, bool choice, CurrencyReader currency, void* context, UErrorCode& status) {
    NameResult result;
    if (U_FAILURE(status)) return result;
    UErrorCode localStatus = U_ZERO_ERROR;
    const auto name = ulocimp_getName(locale, localStatus);
    if (U_FAILURE(localStatus)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return result;
    }
    std::array<char, 4> code{};
    u_UCharsToChars(currency(context, false), code.data(), 3);
    T_CString_toUpperCase(code.data());
    ResourceSet resources;
    try {
        localStatus = U_ZERO_ERROR;
        auto* bundle = ures_open(U_ICUDATA_CURR, name.data(), &localStatus);
        resources.items[0] = bundle;
        if (style >= 2) {
            constexpr const char* keys[] = {"Currencies%narrow", "Currencies%formal", "Currencies%variant"};
            icu::CharString key;
            key.append(keys[style - 2], localStatus).append("/", localStatus).append(code.data(), localStatus);
            result.text = ures_getStringByKeyWithFallback(bundle, key.data(), &result.length, &localStatus);
            if (localStatus == U_MISSING_RESOURCE_ERROR) {
                status = U_USING_FALLBACK_WARNING;
                localStatus = U_ZERO_ERROR;
                style = 0;
            }
        }
        if (!result.text) {
            ures_getByKey(bundle, "Currencies", bundle, &localStatus);
            ures_getByKeyWithFallback(bundle, code.data(), bundle, &localStatus);
            result.text = ures_getStringByIndex(bundle, style, &result.length, &localStatus);
        }
        NameWarning(localStatus, status);
        if (choice) result.choice = 0;
        if (U_FAILURE(localStatus)) {
            result.text = currency(context, true);
            result.length = u_strlen(result.text);
            status = U_USING_DEFAULT_WARNING;
            result.inputAlias = true;
        }
        resources.Close();
        return result;
    } catch (...) {
        resources.Close();
        throw;
    }
}

NameResult PluralName(const char* locale, bool choice, CurrencyReader currency, PluralReader plural, void* context, UErrorCode& status) {
    NameResult result;
    if (U_FAILURE(status)) return result;
    UErrorCode localStatus = U_ZERO_ERROR;
    const auto name = ulocimp_getName(locale, localStatus);
    if (U_FAILURE(localStatus)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return result;
    }
    std::array<char, 4> code{};
    u_UCharsToChars(currency(context, false), code.data(), 3);
    ResourceSet resources;
    try {
        localStatus = U_ZERO_ERROR;
        auto* bundle = ures_open(U_ICUDATA_CURR, name.data(), &localStatus);
        resources.items[0] = bundle;
        ures_getByKey(bundle, "CurrencyPlurals", bundle, &localStatus);
        auto* entry = ures_getByKeyWithFallback(bundle, code.data(), bundle, &localStatus);
        const auto* keyword = U_SUCCESS(localStatus) ? plural(context) : nullptr;
        result.text = ures_getStringByKeyWithFallback(entry, keyword, &result.length, &localStatus);
        if (U_FAILURE(localStatus)) {
            localStatus = U_ZERO_ERROR;
            result.text = ures_getStringByKeyWithFallback(entry, "other", &result.length, &localStatus);
            if (U_FAILURE(localStatus)) {
                result = BasicName(locale, 1, choice, currency, context, status);
                resources.Close();
                return result;
            }
        }
        NameWarning(localStatus, status);
        if (U_FAILURE(localStatus)) {
            result.text = currency(context, true);
            result.length = u_strlen(result.text);
            status = U_USING_DEFAULT_WARNING;
            result.inputAlias = true;
        }
        resources.Close();
        return result;
    } catch (...) {
        resources.Close();
        throw;
    }
}

struct KeywordResources {
    UResourceBundle* bundle = nullptr;
    std::array<UResourceBundle, 4> fields{};
    bool closed = false;
    KeywordResources() {
        for (auto& field : fields) ures_initStackObject(&field);
    }
    void Close() {
        if (closed) return;
        closed = true;
        for (auto iterator = fields.rbegin(); iterator != fields.rend(); ++iterator) ures_close(&*iterator);
        ures_close(std::exchange(bundle, nullptr));
    }
};

const UEnumeration KeywordEnumeration = {
    nullptr, nullptr, ulist_close_keyword_values_iterator, ulist_count_keyword_values,
    uenum_unextDefault, ulist_next_keyword_value, ulist_reset_keyword_values_iterator
};

UEnumeration* KeywordValues(const char* locale, UBool commonlyUsed, UErrorCode& status) {
    const auto region = ulocimp_getRegionForSupplementalData(locale, true, status);
    auto* values = ulist_createEmptyList(&status);
    auto* otherValues = ulist_createEmptyList(&status);
    auto* enumeration = static_cast<UEnumeration*>(uprv_malloc(sizeof(UEnumeration)));
    if (U_FAILURE(status) || !enumeration) {
        if (!enumeration) status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(enumeration);
        ulist_deleteList(values);
        ulist_deleteList(otherValues);
        return nullptr;
    }
    std::memcpy(enumeration, &KeywordEnumeration, sizeof(UEnumeration));
    enumeration->context = values;
    KeywordResources resources;
    try {
        resources.bundle = ures_openDirect(U_ICUDATA_CURR, "supplementalData", &status);
        auto* map = ures_getByKey(resources.bundle, "CurrencyMap", resources.bundle, &status);
        auto* item = &resources.fields[0];
        auto* country = &resources.fields[1];
        auto* currency = &resources.fields[2];
        auto* end = &resources.fields[3];
        while (U_SUCCESS(status) && ures_hasNext(map)) {
            ures_getNextResource(map, item, &status);
            if (U_FAILURE(status)) break;
            const auto* name = ures_getKey(item);
            const bool preferred = region == name;
            if (!preferred && commonlyUsed) continue;
            ures_getByKey(map, name, country, &status);
            while (U_SUCCESS(status) && ures_hasNext(country)) {
                ures_getNextResource(country, currency, &status);
                if (ures_getType(currency) != URES_TABLE) continue;
                auto* code = static_cast<char*>(uprv_malloc(ULOC_KEYWORDS_CAPACITY));
                if (!code) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                std::int32_t length = ULOC_KEYWORDS_CAPACITY;
                ures_getUTF8StringByKey(currency, "id", code, &length, true, &status);
                if (U_FAILURE(status)) {
                    uprv_free(code);
                    break;
                }
                ures_getByKey(currency, "to", end, &status);
                const bool hasEnd = U_SUCCESS(status);
                if (status == U_MISSING_RESOURCE_ERROR) status = U_ZERO_ERROR;
                if (U_FAILURE(status)) {
                    uprv_free(code);
                    break;
                }
                if (preferred && !hasEnd && !ulist_containsString(values, code, static_cast<std::int32_t>(std::strlen(code)))) {
                    ulist_addItemEndList(values, code, true, &status);
                } else if (!commonlyUsed && !ulist_containsString(otherValues, code, static_cast<std::int32_t>(std::strlen(code)))) {
                    ulist_addItemEndList(otherValues, code, true, &status);
                } else {
                    uprv_free(code);
                }
            }
        }
        if (U_SUCCESS(status)) {
            if (commonlyUsed && ulist_getListSize(values) == 0) {
                uenum_close(enumeration);
                enumeration = nullptr;
                enumeration = KeywordValues("und", true, status);
            } else if (!commonlyUsed) {
                ulist_resetList(otherValues);
                while (auto* value = static_cast<char*>(ulist_getNext(otherValues))) {
                    if (ulist_containsString(values, value, static_cast<std::int32_t>(std::strlen(value)))) continue;
                    auto* copy = static_cast<char*>(uprv_malloc(ULOC_KEYWORDS_CAPACITY));
                    if (!copy) {
                        status = U_MEMORY_ALLOCATION_ERROR;
                        break;
                    }
                    std::memcpy(copy, value, std::strlen(value) + 1);
                    ulist_addItemEndList(values, copy, true, &status);
                    if (U_FAILURE(status)) break;
                }
            }
            if (enumeration) ulist_resetList(static_cast<UList*>(enumeration->context));
        } else {
            uenum_close(enumeration);
            enumeration = nullptr;
        }
        resources.Close();
        ulist_deleteList(otherValues);
        return enumeration;
    } catch (...) {
        resources.Close();
        uenum_close(enumeration);
        ulist_deleteList(otherValues);
        throw;
    }
}

HistoricalResult Historical(const char* locale, double date, std::int32_t index, std::int32_t capacity, bool select, UErrorCode& status) {
    HistoricalResult result;
    if (U_FAILURE(status)) return result;
    auto region = ulocimp_getRegionForSupplementalData(locale, false, status);
    if (U_FAILURE(status)) return result;
    if (const auto delimiter = std::strchr(region.data(), '_')) region.truncate(delimiter - region.data());
    UErrorCode localStatus = U_ZERO_ERROR;
    ResourceSet resources;
    try {
        resources.items[0] = ures_openDirect(U_ICUDATA_CURR, "supplementalData", &localStatus);
        resources.items[1] = ures_getByKey(resources.items[0], "CurrencyMap", nullptr, &localStatus);
        resources.items[2] = ures_getByKey(resources.items[1], region.data(), nullptr, &localStatus);
        const auto count = U_SUCCESS(localStatus) ? ures_getSize(resources.items[2]) : 0;
        if (U_SUCCESS(localStatus) && select && (index <= 0 || index > count)) {
            resources.Close();
            return result;
        }
        for (std::int32_t item = 0; U_SUCCESS(localStatus) && item < count; ++item) {
            resources.items[3] = ures_getByIndex(resources.items[2], item, nullptr, &localStatus);
            const auto from = ReadDate(resources, "from", U_DATE_MIN, localStatus);
            const auto to = ReadDate(resources, "to", U_DATE_MAX, localStatus);
            if (U_SUCCESS(localStatus) && from.value <= date && (!to.present || date < to.value)) {
                ++result.count;
                if (select && result.count == index) {
                    std::int32_t length = 0;
                    const auto* code = ures_getStringByKey(resources.items[3], "id", &length, &localStatus);
                    if (U_SUCCESS(localStatus)) {
                        if (length != 3 || !code) NotImplemented_nid_no_patch("ucurr: non-ISO historical currency data");
                        if (capacity > length) {
                            std::copy_n(code, length, result.selected.text.data());
                            result.selected.length = length;
                            result.selected.written = length + 1;
                        }
                    }
                    break;
                }
            }
            ures_close(std::exchange(resources.items[3], nullptr));
        }
        if (status == U_ZERO_ERROR || localStatus != U_ZERO_ERROR) status = localStatus;
        resources.Close();
        return result;
    } catch (...) {
        resources.Close();
        throw;
    }
}

}

void SetDefault_nid_no_patch(const char* locale, std::int32_t& error) {
    if (error > 0 || std::strcmp(uloc_getDefault(), locale) == 0) return;
    auto status = static_cast<UErrorCode>(error);
    uloc_setDefault(locale, &status);
    error = status;
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
    auto result = Historical(locale, date, index, capacity, true, status).selected;
    error = status;
    return result;
}

NameResult Name_nid_no_patch(const char* locale, std::int32_t style, bool pluralName, bool choice, CurrencyReader currency, PluralReader plural, void* context, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    auto result = pluralName ? PluralName(locale, choice, currency, plural, context, status)
        : BasicName(locale, style, choice, currency, context, status);
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
    const auto result = Historical(locale, date, 0, 0, false, status).count;
    error = status;
    return U_SUCCESS(status) ? result : 0;
}

void* OpenISO_nid_no_patch(std::uint32_t type, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    auto* result = ucurr_openISOCurrencies(type, &status);
    error = status;
    return result;
}

void* OpenKeywords_nid_no_patch(const char* locale, std::int8_t commonlyUsed, std::int32_t& error) {
    auto status = static_cast<UErrorCode>(error);
    auto* result = KeywordValues(locale, commonlyUsed, status);
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
