#include "prx/libSceIcu/CaseProperties.hpp"
#include "prx/libc/include/General.hpp"
#include "ucase.h"
#include <cstdint>

namespace {

using FullMapping = UChar32 (*)(UChar32, UCaseContextIterator*, void*, const UChar**, int32_t);

struct CaseContext {
    Icu::CaseContextIterator iterator;
    void* context;
};

UChar32 U_CALLCONV ForwardCaseContext(void* context, int8_t direction) {
    const auto& guest = *static_cast<const CaseContext*>(context);
    return guest.iterator(guest.context, direction);
}

const char16_t** Output(const char16_t** string) {
    if (!string) APS5_INVALID_ARG_EX;
    return string;
}

std::int32_t MapFull(FullMapping map, std::int32_t character, Icu::CaseContextIterator iterator, void* context,
                     const char16_t** string, std::int32_t caseLocale) {
    auto* output = Output(string);
    if (!iterator) return map(character, nullptr, context, output, caseLocale);
    CaseContext guest{iterator, context};
    return map(character, ForwardCaseContext, &guest, output, caseLocale);
}

}

extern "C" {

std::int32_t APS5_VABI ucase_getType_nid_postfix(std::int32_t character) {
    return ucase_getType(character);
}

std::int32_t APS5_VABI ucase_getTypeOrIgnorable_nid_postfix(std::int32_t character) {
    return ucase_getTypeOrIgnorable(character);
}

std::int8_t APS5_VABI ucase_isSoftDotted_nid_postfix(std::int32_t character) {
    return ucase_isSoftDotted(character);
}

std::int8_t APS5_VABI ucase_isCaseSensitive_nid_postfix(std::int32_t character) {
    return ucase_isCaseSensitive(character);
}

std::int32_t APS5_VABI ucase_tolower_nid_postfix(std::int32_t character) {
    return ucase_tolower(character);
}

std::int32_t APS5_VABI ucase_toupper_nid_postfix(std::int32_t character) {
    return ucase_toupper(character);
}

std::int32_t APS5_VABI ucase_totitle_nid_postfix(std::int32_t character) {
    return ucase_totitle(character);
}

std::int32_t APS5_VABI ucase_fold_nid_postfix(std::int32_t character, std::uint32_t options) {
    return ucase_fold(character, options);
}

std::int32_t APS5_VABI ucase_toFullLower_nid_postfix(std::int32_t character, Icu::CaseContextIterator iterator, void* context,
                                                     const char16_t** string, std::int32_t caseLocale) {
    return MapFull(ucase_toFullLower, character, iterator, context, string, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullUpper_nid_postfix(std::int32_t character, Icu::CaseContextIterator iterator, void* context,
                                                     const char16_t** string, std::int32_t caseLocale) {
    return MapFull(ucase_toFullUpper, character, iterator, context, string, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullTitle_nid_postfix(std::int32_t character, Icu::CaseContextIterator iterator, void* context,
                                                     const char16_t** string, std::int32_t caseLocale) {
    return MapFull(ucase_toFullTitle, character, iterator, context, string, caseLocale);
}

std::int32_t APS5_VABI ucase_toFullFolding_nid_postfix(std::int32_t character, const char16_t** string, std::uint32_t options) {
    return ucase_toFullFolding(character, Output(string), options);
}

}
