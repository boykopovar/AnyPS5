#include "prx/libSceIcu/CaseMapping.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "ucase.h"
#include "ucasemap_imp.h"
#include "unicode/ustring.h"
#include "unicode/ucasemap.h"
#include <cstdint>

namespace {

void CheckString(const char16_t* text, std::int32_t length) {
    if (length < -1 || (!text && length != 0)) APS5_INVALID_ARG_EX;
}

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

std::int32_t CaseLocale(const char* locale) {
    if (!locale) locale = Icu::DefaultLocaleName();
    return *locale ? ucase_getCaseLocale(locale) : UCASE_LOC_ROOT;
}

std::int32_t MapInLocale(const char* locale, char16_t* destination, std::int32_t capacity, const char16_t* source,
                         std::int32_t length, UStringCaseMapper* mapper, std::int32_t* error) {
    auto& status = Status(error);
    return ustrcase_mapWithOverlap(CaseLocale(locale), 0, UCASEMAP_BREAK_ITERATOR_NULL destination, capacity, source, length,
                                   mapper, status);
}

bool ReachesEdits(UErrorCode status, const char16_t* source, std::int32_t length, const char16_t* destination,
                  std::int32_t capacity) {
    if (U_FAILURE(status) || capacity < 0 || (!destination && capacity > 0) || !source || length < -1) return false;
    if (length == -1) length = u_strlen(source);
    if (!destination) return true;
    return (source < destination || source >= destination + capacity) && (destination < source || destination >= source + length);
}

std::int32_t MapWithoutOverlap(const char* name, std::int32_t caseLocale, std::uint32_t options, const char16_t* source,
                               std::int32_t length, char16_t* destination, std::int32_t capacity, void* edits,
                               std::int32_t* error, UStringCaseMapper* mapper) {
    auto& status = Status(error);
    if (edits && ReachesEdits(status, source, length, destination, capacity)) NotImplemented_nid_no_patch(name);
    return ustrcase_map(caseLocale, options, UCASEMAP_BREAK_ITERATOR_NULL destination, capacity, source, length, mapper,
                        nullptr, status);
}

}

extern "C" {

std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                 std::int32_t length, std::uint32_t options, std::int32_t* error) {
    return u_strFoldCase(destination, capacity, source, length, options, &Status(error));
}

std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return MapInLocale(locale, destination, capacity, source, length, ustrcase_internalToLower, error);
}

std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return MapInLocale(locale, destination, capacity, source, length, ustrcase_internalToUpper, error);
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

std::int32_t APS5_VABI _ZN6icu_687CaseMap7toLowerEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(
    const char* locale, std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination,
    std::int32_t capacity, void* edits, std::int32_t* error) {
    return MapWithoutOverlap(__func__, CaseLocale(locale), options, source, length, destination, capacity, edits, error,
                             ustrcase_internalToLower);
}

std::int32_t APS5_VABI _ZN6icu_687CaseMap7toUpperEPKcjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(
    const char* locale, std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination,
    std::int32_t capacity, void* edits, std::int32_t* error) {
    return MapWithoutOverlap(__func__, CaseLocale(locale), options, source, length, destination, capacity, edits, error,
                             ustrcase_internalToUpper);
}

std::int32_t APS5_VABI _ZN6icu_687CaseMap4foldEjPKDsiPDsiPNS_5EditsER10UErrorCode_nid_postfix(
    std::uint32_t options, const char16_t* source, std::int32_t length, char16_t* destination, std::int32_t capacity,
    void* edits, std::int32_t* error) {
    return MapWithoutOverlap(__func__, UCASE_LOC_ROOT, options, source, length, destination, capacity, edits, error,
                             ustrcase_internalFold);
}

Icu::CaseMap* APS5_VABI ucasemap_open_nid_postfix(const char* locale, std::uint32_t options, std::int32_t* error) {
    return reinterpret_cast<Icu::CaseMap*>(ucasemap_open(locale, options, &Status(error)));
}

std::int32_t APS5_VABI ucasemap_utf8FoldCase_nid_postfix(const Icu::CaseMap* caseMap, char* destination,
                                                         std::int32_t capacity, const char* source,
                                                         std::int32_t length, std::int32_t* error) {
    return ucasemap_utf8FoldCase(reinterpret_cast<const UCaseMap*>(caseMap), destination, capacity, source, length,
                                &Status(error));
}

std::int32_t APS5_VABI ucasemap_utf8ToUpper_nid_postfix(const Icu::CaseMap* caseMap, char* destination,
                                                        std::int32_t capacity, const char* source,
                                                        std::int32_t length, std::int32_t* error) {
    return ucasemap_utf8ToUpper(reinterpret_cast<const UCaseMap*>(caseMap), destination, capacity, source, length,
                               &Status(error));
}

}
