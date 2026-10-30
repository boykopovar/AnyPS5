#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "ucase.h"
#include "ucasemap_imp.h"
#include "unicode/ustring.h"
#include <cstdint>

namespace {

void CheckString(const char16_t* text, std::int32_t length) {
    if (length < -1 || (!text && length != 0)) APS5_INVALID_ARG_EX;
}

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

bool InvalidMapping(const char16_t* destination, std::int32_t capacity, const char16_t* source, std::int32_t length) {
    return capacity < 0 || (!destination && capacity > 0) || !source || length < -1;
}

std::int32_t MapInLocale(const char* name, const char* locale, char16_t* destination, std::int32_t capacity,
                         const char16_t* source, std::int32_t length, UStringCaseMapper* mapper, std::int32_t* error) {
    auto& status = Status(error);
    std::int32_t caseLocale = UCASE_LOC_ROOT;
    if (locale) {
        if (*locale) caseLocale = ucase_getCaseLocale(locale);
    } else if (U_SUCCESS(status) && !InvalidMapping(destination, capacity, source, length)) {
        NotImplemented_nid_no_patch(name);
    }
    return ustrcase_mapWithOverlap(caseLocale, 0, UCASEMAP_BREAK_ITERATOR_NULL destination, capacity, source, length, mapper,
                                   status);
}

}

extern "C" {

std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                 std::int32_t length, std::uint32_t options, std::int32_t* error) {
    return u_strFoldCase(destination, capacity, source, length, options, &Status(error));
}

std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return MapInLocale("u_strToLower: default locale", locale, destination, capacity, source, length,
                       ustrcase_internalToLower, error);
}

std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return MapInLocale("u_strToUpper: default locale", locale, destination, capacity, source, length,
                       ustrcase_internalToUpper, error);
}

std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second,
                                                    std::int32_t secondLength, std::uint32_t options, std::int32_t* error) {
    return u_strCaseCompare(first, firstLength, second, secondLength, options, reinterpret_cast<UErrorCode*>(error));
}

std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::uint32_t options) {
    CheckString(first, -1);
    CheckString(second, -1);
    return u_strcasecmp(first, second, options);
}

std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count,
                                                 std::uint32_t options) {
    CheckString(first, count);
    CheckString(second, count);
    return u_strncasecmp(first, second, count, options);
}

std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t length,
                                                std::uint32_t options) {
    CheckString(first, length);
    CheckString(second, length);
    return u_memcasecmp(first, second, length, options);
}

void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second,
                                                       std::int32_t secondLength, std::uint32_t options,
                                                       std::int32_t* firstMatch, std::int32_t* secondMatch,
                                                       std::int32_t* error) {
    if (U_FAILURE(Status(error))) return;
    if (firstMatch && !secondMatch) APS5_INVALID_ARG_EX;
    CheckString(first, firstLength);
    CheckString(second, secondLength);
    u_caseInsensitivePrefixMatch(first, firstLength, second, secondLength, options, firstMatch, secondMatch,
                                 reinterpret_cast<UErrorCode*>(error));
}

}
