#include "prx/libSceIcu/Converter.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<UChar, char16_t>);
static_assert(sizeof(UErrorCode) == sizeof(std::int32_t));
static_assert(sizeof(UBool) == sizeof(std::int8_t));

namespace {

template<class Call>
auto WithStatus(std::int32_t* guestStatus, Call call) {
    if (!guestStatus) APS5_INVALID_ARG_EX;
    UErrorCode status = static_cast<UErrorCode>(*guestStatus);
    if constexpr (std::is_void_v<decltype(call(&status))>) {
        call(&status);
        *guestStatus = static_cast<std::int32_t>(status);
    } else {
        auto value = call(&status);
        *guestStatus = static_cast<std::int32_t>(status);
        return value;
    }
}

}

extern "C" {

UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status) {
    return WithStatus(status, [name](UErrorCode* error) { return ucnv_open(name, error); });
}

void APS5_VABI ucnv_close_nid_postfix(UConverter* converter) {
    ucnv_close(converter);
}

const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status) {
    return WithStatus(status, [converter](UErrorCode* error) { return ucnv_getName(converter, error); });
}

void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter) {
    ucnv_reset(converter);
}

void APS5_VABI ucnv_resetToUnicode_nid_postfix(UConverter* converter) {
    ucnv_resetToUnicode(converter);
}

void APS5_VABI ucnv_resetFromUnicode_nid_postfix(UConverter* converter) {
    ucnv_resetFromUnicode(converter);
}

void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int8_t enabled) {
    ucnv_setFallback(converter, static_cast<UBool>(enabled));
}

void APS5_VABI ucnv_setSubstChars_nid_postfix(UConverter* converter, const char* characters, std::int8_t length, std::int32_t* status) {
    WithStatus(status, [=](UErrorCode* error) { ucnv_setSubstChars(converter, characters, length, error); });
}

std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(UConverter* converter, char16_t* destination, std::int32_t capacity,
                                                const char* source, std::int32_t length, std::int32_t* status) {
    return WithStatus(status, [=](UErrorCode* error) { return ucnv_toUChars(converter, destination, capacity, source, length, error); });
}

std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination, std::int32_t capacity,
                                                  const char16_t* source, std::int32_t length, std::int32_t* status) {
    return WithStatus(status, [=](UErrorCode* error) { return ucnv_fromUChars(converter, destination, capacity, source, length, error); });
}

void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** destination, const char16_t* destinationLimit,
                                         const char** source, const char* sourceLimit, std::int32_t* offsets,
                                         std::int8_t flush, std::int32_t* status) {
    WithStatus(status, [=](UErrorCode* error) {
        ucnv_toUnicode(converter, destination, destinationLimit, source, sourceLimit, offsets, static_cast<UBool>(flush), error);
    });
}

void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** destination, const char* destinationLimit,
                                           const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets,
                                           std::int8_t flush, std::int32_t* status) {
    WithStatus(status, [=](UErrorCode* error) {
        ucnv_fromUnicode(converter, destination, destinationLimit, source, sourceLimit, offsets, static_cast<UBool>(flush), error);
    });
}

void APS5_VABI ucnv_convertEx_nid_postfix(UConverter* destinationConverter, UConverter* sourceConverter,
                                         char** destination, const char* destinationLimit, const char** source, const char* sourceLimit,
                                         char16_t* pivotStart, char16_t** pivotSource, char16_t** pivotDestination,
                                         const char16_t* pivotLimit, std::int8_t reset, std::int8_t flush, std::int32_t* status) {
    WithStatus(status, [=](UErrorCode* error) {
        ucnv_convertEx(destinationConverter, sourceConverter, destination, destinationLimit, source, sourceLimit,
                       pivotStart, pivotSource, pivotDestination, pivotLimit, static_cast<UBool>(reset), static_cast<UBool>(flush), error);
    });
}

}
