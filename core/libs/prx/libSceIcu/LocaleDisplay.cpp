// Derived from icu4c/source/common/uloc.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2)
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "prx/libSceIcu/LocaleDisplay.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uloc.h"
#include <cerrno>
#include <cstdint>
#include <string>

namespace {

constexpr std::int32_t MaximumCapacity = 8192;
constexpr std::size_t MinimumText = 64;
constexpr std::size_t MaximumText = 4096;

bool ReadStatus(const std::int32_t* status, UErrorCode& local, const char* caller) {
    if (!status) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    local = static_cast<UErrorCode>(value);
    return true;
}

void WriteStatus(std::int32_t* status, UErrorCode local, const char* caller) {
    if (!status) return;
    const std::int32_t value = static_cast<std::int32_t>(local);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

bool ReadText(const char* source, std::string& destination) {
    if (!source) return true;
    for (std::size_t capacity = MinimumText; capacity <= MaximumText; capacity *= 2) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(source, capacity, destination);
        errno = savedErrno;
        if (error == 0) return true;
        if (error != 63) return false;
    }
    return false;
}

bool ValidCapacity(const char16_t* destination, std::int32_t capacity) {
    if (capacity < 0 || capacity > MaximumCapacity) return false;
    return capacity == 0 || destination != nullptr;
}

std::int32_t Reject(std::int32_t* status, const char* caller) {
    WriteStatus(status, U_ILLEGAL_ARGUMENT_ERROR, caller);
    return 0;
}

void CopyOut(char16_t* destination, const char16_t* source, std::int32_t length, std::int32_t capacity) {
    const std::int32_t written = length < capacity ? length : capacity;
    if (written <= 0) return;
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(destination, source, static_cast<std::size_t>(written) * sizeof(char16_t));
    errno = savedErrno;
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getDisplayName_nid_postfix(const char* localeID, const char* inLocaleID,
    char16_t* result, std::int32_t maxResultSize, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(result, maxResultSize)) return Reject(status, __func__);
    std::string locale;
    std::string displayLocale;
    if (!ReadText(localeID, locale) || !ReadText(inLocaleID, displayLocale)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayName(localeID ? locale.c_str() : nullptr,
        inLocaleID ? displayLocale.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), maxResultSize, &local);
    if (local <= 0) CopyOut(result, buffer, length, maxResultSize);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayLanguage_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* language, std::int32_t languageCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(language, languageCapacity)) return Reject(status, __func__);
    std::string localeText;
    std::string displayLocaleText;
    if (!ReadText(locale, localeText) || !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayLanguage(locale ? localeText.c_str() : nullptr,
        displayLocale ? displayLocaleText.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), languageCapacity, &local);
    if (local <= 0) CopyOut(language, buffer, length, languageCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayScript_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* script, std::int32_t scriptCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(script, scriptCapacity)) return Reject(status, __func__);
    std::string localeText;
    std::string displayLocaleText;
    if (!ReadText(locale, localeText) || !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayScript(locale ? localeText.c_str() : nullptr,
        displayLocale ? displayLocaleText.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), scriptCapacity, &local);
    if (local <= 0) CopyOut(script, buffer, length, scriptCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayCountry_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* country, std::int32_t countryCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(country, countryCapacity)) return Reject(status, __func__);
    std::string localeText;
    std::string displayLocaleText;
    if (!ReadText(locale, localeText) || !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayCountry(locale ? localeText.c_str() : nullptr,
        displayLocale ? displayLocaleText.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), countryCapacity, &local);
    if (local <= 0) CopyOut(country, buffer, length, countryCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayVariant_nid_postfix(const char* locale, const char* displayLocale,
    char16_t* variant, std::int32_t variantCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(variant, variantCapacity)) return Reject(status, __func__);
    std::string localeText;
    std::string displayLocaleText;
    if (!ReadText(locale, localeText) || !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayVariant(locale ? localeText.c_str() : nullptr,
        displayLocale ? displayLocaleText.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), variantCapacity, &local);
    if (local <= 0) CopyOut(variant, buffer, length, variantCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayKeyword_nid_postfix(const char* keyword, const char* displayLocale,
    char16_t* destination, std::int32_t destinationCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(destination, destinationCapacity)) return Reject(status, __func__);
    std::string keywordText;
    std::string displayLocaleText;
    if (!ReadText(keyword, keywordText) || !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayKeyword(keyword ? keywordText.c_str() : nullptr,
        displayLocale ? displayLocaleText.c_str() : nullptr, reinterpret_cast<UChar*>(buffer), destinationCapacity,
        &local);
    if (local <= 0) CopyOut(destination, buffer, length, destinationCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getDisplayKeywordValue_nid_postfix(const char* locale, const char* keyword,
    const char* displayLocale, char16_t* destination, std::int32_t destinationCapacity, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    if (!ValidCapacity(destination, destinationCapacity)) return Reject(status, __func__);
    std::string localeText;
    std::string keywordText;
    std::string displayLocaleText;
    if (!ReadText(locale, localeText) || !ReadText(keyword, keywordText) ||
        !ReadText(displayLocale, displayLocaleText)) return Reject(status, __func__);
    char16_t buffer[MaximumCapacity + 1] = {};
    const std::int32_t length = uloc_getDisplayKeywordValue(locale ? localeText.c_str() : nullptr,
        keyword ? keywordText.c_str() : nullptr, displayLocale ? displayLocaleText.c_str() : nullptr,
        reinterpret_cast<UChar*>(buffer), destinationCapacity, &local);
    if (local <= 0) CopyOut(destination, buffer, length, destinationCapacity);
    WriteStatus(status, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_getLineOrientation_nid_postfix(const char* localeID, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return static_cast<std::int32_t>(ULOC_LAYOUT_UNKNOWN);
    std::string locale;
    if (!ReadText(localeID, locale)) {
        WriteStatus(status, U_ILLEGAL_ARGUMENT_ERROR, __func__);
        return static_cast<std::int32_t>(ULOC_LAYOUT_UNKNOWN);
    }
    const ULayoutType orientation = uloc_getLineOrientation(localeID ? locale.c_str() : nullptr, &local);
    WriteStatus(status, local, __func__);
    return static_cast<std::int32_t>(orientation);
}

}
