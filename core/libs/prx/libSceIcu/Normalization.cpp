#include "prx/libSceIcu/Normalization.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/unorm2.h"
#include <cstdint>
#include <cstring>

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

const UNormalizer2* Native(const Icu::Normalizer2* normalizer) {
    return reinterpret_cast<const UNormalizer2*>(normalizer);
}

const UNormalizer2* Required(const Icu::Normalizer2* normalizer) {
    if (!normalizer) APS5_INVALID_ARG_EX;
    return Native(normalizer);
}

const Icu::Normalizer2* Guest(const UNormalizer2* normalizer) {
    return reinterpret_cast<const Icu::Normalizer2*>(normalizer);
}

bool InvalidSource(const char16_t* source, std::int32_t length) {
    return source == nullptr ? length != 0 : length < -1;
}

bool InvalidDestination(const char16_t* destination, std::int32_t capacity) {
    return destination == nullptr ? capacity != 0 : capacity < 0;
}

bool InvalidQuery(const char16_t* text, std::int32_t length) {
    return (text == nullptr && length != 0) || length < -1;
}

bool InvalidAppend(const char16_t* first, std::int32_t firstLength, std::int32_t capacity, const char16_t* second,
                   std::int32_t secondLength) {
    return InvalidSource(second, secondLength) ||
           (first == nullptr ? (capacity != 0 || firstLength != 0) : (capacity < 0 || firstLength < -1)) ||
           (first == second && first != nullptr);
}

}

extern "C" {

const Icu::Normalizer2* APS5_VABI unorm2_getNFCInstance_nid_postfix(std::int32_t* error) {
    return Guest(unorm2_getNFCInstance(&Status(error)));
}

const Icu::Normalizer2* APS5_VABI unorm2_getNFDInstance_nid_postfix(std::int32_t* error) {
    return Guest(unorm2_getNFDInstance(&Status(error)));
}

const Icu::Normalizer2* APS5_VABI unorm2_getInstance_nid_postfix(const char* package, const char* name, std::int32_t mode,
                                                                 std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && name && *name && (package || std::strcmp(name, "nfc") != 0)) {
        NotImplemented_nid_no_patch("unorm2_getInstance: normalization data file");
    }
    return Guest(unorm2_getInstance(package, name, static_cast<UNormalization2Mode>(mode), &status));
}

std::int32_t APS5_VABI unorm2_normalize_nid_postfix(const Icu::Normalizer2* normalizer, const char16_t* source,
                                                    std::int32_t length, char16_t* destination, std::int32_t capacity,
                                                    std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && length != 0 && !InvalidSource(source, length) &&
        !InvalidDestination(destination, capacity) && !(source == destination && source != nullptr)) {
        APS5_INVALID_ARG_EX;
    }
    return unorm2_normalize(Native(normalizer), source, length, destination, capacity, &status);
}

std::int32_t APS5_VABI unorm2_normalizeSecondAndAppend_nid_postfix(const Icu::Normalizer2* normalizer, char16_t* first,
                                                                   std::int32_t firstLength, std::int32_t capacity,
                                                                   const char16_t* second, std::int32_t secondLength,
                                                                   std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && secondLength != 0 && !InvalidAppend(first, firstLength, capacity, second, secondLength)) {
        APS5_INVALID_ARG_EX;
    }
    return unorm2_normalizeSecondAndAppend(Native(normalizer), first, firstLength, capacity, second, secondLength, &status);
}

std::int32_t APS5_VABI unorm2_append_nid_postfix(const Icu::Normalizer2* normalizer, char16_t* first, std::int32_t firstLength,
                                                 std::int32_t capacity, const char16_t* second, std::int32_t secondLength,
                                                 std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && secondLength != 0 && !InvalidAppend(first, firstLength, capacity, second, secondLength)) {
        APS5_INVALID_ARG_EX;
    }
    return unorm2_append(Native(normalizer), first, firstLength, capacity, second, secondLength, &status);
}

std::int32_t APS5_VABI unorm2_getDecomposition_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character,
                                                           char16_t* decomposition, std::int32_t capacity, std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && !InvalidDestination(decomposition, capacity)) APS5_INVALID_ARG_EX;
    return unorm2_getDecomposition(Native(normalizer), character, decomposition, capacity, &status);
}

std::int32_t APS5_VABI unorm2_getRawDecomposition_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character,
                                                              char16_t* decomposition, std::int32_t capacity,
                                                              std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && !InvalidDestination(decomposition, capacity)) APS5_INVALID_ARG_EX;
    return unorm2_getRawDecomposition(Native(normalizer), character, decomposition, capacity, &status);
}

std::int32_t APS5_VABI unorm2_composePair_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t first, std::int32_t second) {
    return unorm2_composePair(Required(normalizer), first, second);
}

std::uint8_t APS5_VABI unorm2_getCombiningClass_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character) {
    return unorm2_getCombiningClass(Required(normalizer), character);
}

std::int8_t APS5_VABI unorm2_isNormalized_nid_postfix(const Icu::Normalizer2* normalizer, const char16_t* text,
                                                      std::int32_t length, std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && !InvalidQuery(text, length)) APS5_INVALID_ARG_EX;
    return unorm2_isNormalized(Native(normalizer), text, length, &status);
}

std::int32_t APS5_VABI unorm2_quickCheck_nid_postfix(const Icu::Normalizer2* normalizer, const char16_t* text, std::int32_t length,
                                                     std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && !InvalidQuery(text, length)) APS5_INVALID_ARG_EX;
    return unorm2_quickCheck(Native(normalizer), text, length, &status);
}

std::int32_t APS5_VABI unorm2_spanQuickCheckYes_nid_postfix(const Icu::Normalizer2* normalizer, const char16_t* text,
                                                            std::int32_t length, std::int32_t* error) {
    auto& status = Status(error);
    if (U_SUCCESS(status) && !normalizer && !InvalidQuery(text, length)) APS5_INVALID_ARG_EX;
    return unorm2_spanQuickCheckYes(Native(normalizer), text, length, &status);
}

std::int8_t APS5_VABI unorm2_hasBoundaryBefore_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character) {
    return unorm2_hasBoundaryBefore(Required(normalizer), character);
}

std::int8_t APS5_VABI unorm2_hasBoundaryAfter_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character) {
    return unorm2_hasBoundaryAfter(Required(normalizer), character);
}

std::int8_t APS5_VABI unorm2_isInert_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t character) {
    return unorm2_isInert(Required(normalizer), character);
}

}
