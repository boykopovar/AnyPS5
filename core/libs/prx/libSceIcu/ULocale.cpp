#include "prx/libSceIcu/ULocale.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ulocale.h"
#include "unicode/uenum.h"

#include <new>

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

UEnumeration* Native(UEnumeration* enumeration) {
    return static_cast<UEnumeration*>(enumeration->context);
}

void APS5_VABI CloseEnumeration(UEnumeration* enumeration) {
    uenum_close(Native(enumeration));
    delete enumeration;
}

std::int32_t APS5_VABI CountEnumeration(UEnumeration* enumeration, std::int32_t* error) {
    return uenum_count(Native(enumeration), reinterpret_cast<UErrorCode*>(error));
}

const char16_t* APS5_VABI NextUnicodeEnumeration(UEnumeration* enumeration, std::int32_t* length,
                                                 std::int32_t* error) {
    return uenum_unext(Native(enumeration), length, reinterpret_cast<UErrorCode*>(error));
}

const char* APS5_VABI NextEnumeration(UEnumeration* enumeration, std::int32_t* length, std::int32_t* error) {
    return uenum_next(Native(enumeration), length, reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI ResetEnumeration(UEnumeration* enumeration, std::int32_t* error) {
    uenum_reset(Native(enumeration), reinterpret_cast<UErrorCode*>(error));
}

UEnumeration* Wrap(UEnumeration* native, UErrorCode& status) {
    if (!native) return nullptr;
    auto* enumeration = new (std::nothrow) UEnumeration;
    if (!enumeration) {
        uenum_close(native);
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    *enumeration = {nullptr, native, CloseEnumeration, CountEnumeration, NextUnicodeEnumeration, NextEnumeration,
                    ResetEnumeration};
    return enumeration;
}

}

extern "C" {

ULocale* APS5_VABI ulocale_openForLocaleID_nid_postfix(const char* locale, std::int32_t length,
                                                        std::int32_t* error) {
    return ulocale_openForLocaleID(locale, length, &Status(error));
}

void APS5_VABI ulocale_close_nid_postfix(ULocale* locale) {
    ulocale_close(locale);
}

const char* APS5_VABI ulocale_getLocaleID_nid_postfix(const ULocale* locale) {
    return ulocale_getLocaleID(locale);
}

bool APS5_VABI ulocale_isBogus_nid_postfix(const ULocale* locale) {
    return ulocale_isBogus(locale);
}

ULocale* APS5_VABI ulocale_openForLanguageTag_nid_postfix(const char* tag, std::int32_t length,
                                                           std::int32_t* error) {
    return ulocale_openForLanguageTag(tag, length, &Status(error));
}

const char* APS5_VABI ulocale_getBaseName_nid_postfix(const ULocale* locale) {
    return ulocale_getBaseName(locale);
}

std::int32_t APS5_VABI ulocale_getKeywordValue_nid_postfix(const ULocale* locale, const char* keyword,
                                                            std::int32_t keywordLength, char* value,
                                                            std::int32_t capacity, std::int32_t* error) {
    return ulocale_getKeywordValue(locale, keyword, keywordLength, value, capacity, &Status(error));
}

UEnumeration* APS5_VABI ulocale_getKeywords_nid_postfix(const ULocale* locale, std::int32_t* error) {
    auto& status = Status(error);
    return Wrap(ulocale_getKeywords(locale, &status), status);
}

const char* APS5_VABI ulocale_getLanguage_nid_postfix(const ULocale* locale) {
    return ulocale_getLanguage(locale);
}

const char* APS5_VABI ulocale_getRegion_nid_postfix(const ULocale* locale) {
    return ulocale_getRegion(locale);
}

const char* APS5_VABI ulocale_getScript_nid_postfix(const ULocale* locale) {
    return ulocale_getScript(locale);
}

std::int32_t APS5_VABI ulocale_getUnicodeKeywordValue_nid_postfix(const ULocale* locale, const char* keyword,
                                                                   std::int32_t keywordLength, char* value,
                                                                   std::int32_t capacity, std::int32_t* error) {
    return ulocale_getUnicodeKeywordValue(locale, keyword, keywordLength, value, capacity, &Status(error));
}

UEnumeration* APS5_VABI ulocale_getUnicodeKeywords_nid_postfix(const ULocale* locale, std::int32_t* error) {
    auto& status = Status(error);
    return Wrap(ulocale_getUnicodeKeywords(locale, &status), status);
}

const char* APS5_VABI ulocale_getVariant_nid_postfix(const ULocale* locale) {
    return ulocale_getVariant(locale);
}

}
