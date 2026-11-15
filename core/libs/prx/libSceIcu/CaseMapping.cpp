#include "prx/libSceIcu/CaseMapping.hpp"
#include "prx/libSceIcu/GuestAccess.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "ucase.h"
#include "ucasemap_imp.h"
#include "unicode/ustring.h"
#include <algorithm>
#include <cstdint>
#include <string>

namespace {

void CheckString(const char16_t* text, std::int32_t length) {
    if (length < -1 || (!text && length != 0)) APS5_INVALID_ARG_EX;
}

void StoreStatus(std::int32_t* error, std::int32_t previous, UErrorCode status) {
    if (status != previous) Icu::WriteGuest(error, static_cast<std::int32_t>(status));
}

bool InvalidMapping(const char16_t* destination, std::int32_t capacity, const char16_t* source, std::int32_t length) {
    return capacity < 0 || (!destination && capacity > 0) || !source || length < -1;
}

bool Overlapping(const char16_t* destination, std::int32_t capacity, const char16_t* source, std::size_t length) {
    if (!destination) return false;
    const auto output = reinterpret_cast<std::uintptr_t>(destination);
    const auto input = reinterpret_cast<std::uintptr_t>(source);
    return input >= output ? input - output < static_cast<std::size_t>(capacity) * sizeof(char16_t)
                           : output - input < length * sizeof(char16_t);
}

std::int32_t Map(const char* name, const char* locale, char16_t* destination, std::int32_t capacity,
                 const char16_t* source, std::int32_t length, std::uint32_t options,
                 UStringCaseMapper* mapper, std::int32_t* error) {
    const Icu::SavedErrno savedErrno;
    const auto previous = Icu::ReadGuest(error);
    if (previous > 0) return 0;
    if (InvalidMapping(destination, capacity, source, length)) {
        Icu::WriteGuest(error, Icu::IllegalArgument);
        return 0;
    }
    std::int32_t caseLocale = UCASE_LOC_ROOT;
    if (locale) {
        const auto localeCopy = Icu::CopyText(locale, -1);
        if (!localeCopy.empty()) caseLocale = ucase_getCaseLocale(localeCopy.c_str());
    } else {
        NotImplemented_nid_no_patch(name);
    }
    const auto input = Icu::CopyText(source, length);
    const auto inputLength = static_cast<std::int32_t>(input.size());
    auto status = static_cast<UErrorCode>(previous);
    const auto required = ustrcase_mapWithOverlap(caseLocale, options, UCASEMAP_BREAK_ITERATOR_NULL
        nullptr, 0, input.c_str(), inputLength, mapper, status);
    if (capacity == 0 || (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)) {
        StoreStatus(error, previous, status);
        return required;
    }
    if (required > capacity && Overlapping(destination, capacity, source, input.size())) {
        StoreStatus(error, previous, U_BUFFER_OVERFLOW_ERROR);
        return required;
    }
    const auto outputSize = static_cast<std::size_t>(std::min<std::int64_t>(capacity, static_cast<std::int64_t>(required) + 1));
    std::u16string output(outputSize, 0);
    if (required > capacity && !GuestPtr<const char16_t>(destination).CopyIn(output.data(), output.size()))
        APS5_INVALID_ARG_EX;
    status = static_cast<UErrorCode>(previous);
    const auto result = ustrcase_mapWithOverlap(caseLocale, options, UCASEMAP_BREAK_ITERATOR_NULL
        output.data(), static_cast<std::int32_t>(outputSize), input.c_str(), inputLength, mapper, status);
    const GuestArena::HostWrite outputAccess(destination, output.size() * sizeof(char16_t));
    if (!outputAccess.Open()) APS5_INVALID_ARG_EX;
    Icu::RequireWritable(destination, output.size() * sizeof(char16_t));
    if (!GuestPtr<char16_t>(destination).CopyOut(output.data(), output.size())) APS5_INVALID_ARG_EX;
    StoreStatus(error, previous, status);
    return result;
}

}

extern "C" {

std::int32_t APS5_VABI u_strFoldCase_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                  std::int32_t length, std::uint32_t options, std::int32_t* error) {
    return Map(__func__, "", destination, capacity, source, length, options, ustrcase_internalFold, error);
}

std::int32_t APS5_VABI u_strToLower_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return Map("u_strToLower: default locale", locale, destination, capacity, source, length, 0,
               ustrcase_internalToLower, error);
}

std::int32_t APS5_VABI u_strToUpper_nid_postfix(char16_t* destination, std::int32_t capacity, const char16_t* source,
                                                std::int32_t length, const char* locale, std::int32_t* error) {
    return Map("u_strToUpper: default locale", locale, destination, capacity, source, length, 0,
               ustrcase_internalToUpper, error);
}

std::int32_t APS5_VABI u_strCaseCompare_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second,
                                                    std::int32_t secondLength, std::uint32_t options, std::int32_t* error) {
    const Icu::SavedErrno savedErrno;
    if (!error) return 0;
    const auto previous = Icu::ReadGuest(error);
    if (previous > 0) return 0;
    if (!first || !second || firstLength < -1 || secondLength < -1) {
        Icu::WriteGuest(error, Icu::IllegalArgument);
        return 0;
    }
    const auto firstCopy = Icu::CopyText(first, firstLength);
    const auto secondCopy = Icu::CopyText(second, secondLength);
    auto status = static_cast<UErrorCode>(previous);
    const auto result = u_strCaseCompare(firstCopy.c_str(), firstLength, secondCopy.c_str(), secondLength, options, &status);
    StoreStatus(error, previous, status);
    return result;
}

std::int32_t APS5_VABI u_strcasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::uint32_t options) {
    CheckString(first, -1);
    CheckString(second, -1);
    const Icu::SavedErrno savedErrno;
    const auto firstCopy = Icu::CopyText(first, -1);
    const auto secondCopy = Icu::CopyText(second, -1);
    return u_strcasecmp(firstCopy.c_str(), secondCopy.c_str(), options);
}

std::int32_t APS5_VABI u_strncasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count,
                                                 std::uint32_t options) {
    CheckString(first, count);
    CheckString(second, count);
    const Icu::SavedErrno savedErrno;
    const auto firstCopy = Icu::CopyText(first, count, true);
    const auto secondCopy = Icu::CopyText(second, count, true);
    return u_strncasecmp(firstCopy.c_str(), secondCopy.c_str(), count, options);
}

std::int32_t APS5_VABI u_memcasecmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t length,
                                                std::uint32_t options) {
    CheckString(first, length);
    CheckString(second, length);
    const Icu::SavedErrno savedErrno;
    const auto firstCopy = Icu::CopyText(first, length);
    const auto secondCopy = Icu::CopyText(second, length);
    return u_memcasecmp(firstCopy.c_str(), secondCopy.c_str(), length, options);
}

void APS5_VABI u_caseInsensitivePrefixMatch_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second,
                                                       std::int32_t secondLength, std::uint32_t options,
                                                       std::int32_t* firstMatch, std::int32_t* secondMatch,
                                                       std::int32_t* error) {
    const Icu::SavedErrno savedErrno;
    const auto previous = Icu::ReadGuest(error);
    if (previous > 0) return;
    if (firstMatch && !secondMatch) APS5_INVALID_ARG_EX;
    CheckString(first, firstLength);
    CheckString(second, secondLength);
    const auto firstCopy = Icu::CopyText(first, firstLength);
    const auto secondCopy = Icu::CopyText(second, secondLength);
    auto status = static_cast<UErrorCode>(previous);
    std::int32_t firstResult = 0;
    std::int32_t secondResult = 0;
    u_caseInsensitivePrefixMatch(firstCopy.c_str(), firstLength, secondCopy.c_str(), secondLength, options,
                                 firstMatch ? &firstResult : nullptr, firstMatch ? &secondResult : nullptr, &status);
    if (firstMatch) {
        const GuestArena::HostWrite firstAccess(firstMatch, sizeof(firstResult));
        const GuestArena::HostWrite secondAccess(secondMatch, sizeof(secondResult));
        if (!firstAccess.Open() || !secondAccess.Open()) APS5_INVALID_ARG_EX;
        Icu::RequireWritable(firstMatch, sizeof(firstResult));
        Icu::RequireWritable(secondMatch, sizeof(secondResult));
        Icu::WriteGuest(firstMatch, firstResult);
        Icu::WriteGuest(secondMatch, secondResult);
    }
    StoreStatus(error, previous, status);
}

}
