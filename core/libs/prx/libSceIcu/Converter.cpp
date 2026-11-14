#include "prx/libSceIcu/Converter.hpp"
#include "prx/libc/include/General.hpp"
#include "ucnv_bld.h"
#include "unicode/ucnv.h"
#include <cstdint>

namespace {

constexpr std::int32_t MissingResource = 2;
constexpr std::int32_t FileAccess = 4;
constexpr std::int32_t MaxNameLength = 60;

UConverter* Native(Icu::Converter* converter) {
    return reinterpret_cast<UConverter*>(converter);
}

const UConverter* Native(const Icu::Converter* converter) {
    return reinterpret_cast<const UConverter*>(converter);
}

Icu::Converter* Guest(UConverter* converter) {
    return reinterpret_cast<Icu::Converter*>(converter);
}

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(error);
}

UErrorCode& Required(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *Status(error);
}

template <typename T>
T* Required(T* converter) {
    if (!converter) APS5_INVALID_ARG_EX;
    return converter;
}

bool Ready(const std::int32_t* error) {
    return error && *error <= 0;
}

void RequireData(bool ready, const std::int32_t* error) {
    if (ready && (*error == MissingResource || *error == FileAccess)) NotImplemented_nid_no_patch("libSceIcu: converter data");
}

void RequireName(const char* name) {
    if (!name) NotImplemented_nid_no_patch("libSceIcu: default converter");
}

UConverter* Open(const char* name, std::int32_t* error) {
    const auto ready = Ready(error);
    if (ready) RequireName(name);
    auto* converter = ucnv_open(name, Status(error));
    RequireData(ready, error);
    return converter;
}

bool Empty(const char* source, std::int32_t length) {
    return length == 0 || (length < 0 && *source == 0);
}

bool Converts(const char* target, std::int32_t capacity, const char* source, std::int32_t length) {
    return source && length >= -1 && capacity >= 0 && (capacity == 0 || target) && !Empty(source, length);
}

}

extern "C" {

Icu::Converter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* error) {
    return Guest(Open(name, error));
}

Icu::Converter* APS5_VABI ucnv_openU_nid_postfix(const char16_t* name, std::int32_t* error) {
    if (!Ready(error)) return nullptr;
    if (!name) NotImplemented_nid_no_patch("libSceIcu: default converter");
    std::int32_t length = 0;
    while (name[length] != 0) {
        if (++length >= MaxNameLength) {
            *error = U_ILLEGAL_ARGUMENT_ERROR;
            return nullptr;
        }
    }
    char ascii[MaxNameLength];
    for (std::int32_t index = 0; index <= length; ++index) {
        if (name[index] > 0x7f) NotImplemented_nid_no_patch("libSceIcu: default converter");
        ascii[index] = static_cast<char>(name[index]);
    }
    return Guest(Open(ascii, error));
}

void APS5_VABI ucnv_close_nid_postfix(Icu::Converter* converter) {
    ucnv_close(Native(converter));
}

const char* APS5_VABI ucnv_getName_nid_postfix(const Icu::Converter* converter, std::int32_t* error) {
    auto& status = Required(error);
    if (U_SUCCESS(status)) Required(converter);
    return ucnv_getName(Native(converter), &status);
}

void APS5_VABI ucnv_reset_nid_postfix(Icu::Converter* converter) {
    ucnv_reset(Native(converter));
}

void APS5_VABI ucnv_resetToUnicode_nid_postfix(Icu::Converter* converter) {
    ucnv_resetToUnicode(Native(converter));
}

void APS5_VABI ucnv_resetFromUnicode_nid_postfix(Icu::Converter* converter) {
    ucnv_resetFromUnicode(Native(converter));
}

std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const Icu::Converter* converter) {
    return ucnv_getMaxCharSize(Native(Required(converter)));
}

std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const Icu::Converter* converter) {
    return ucnv_getMinCharSize(Native(Required(converter)));
}

std::int32_t APS5_VABI ucnv_getType_nid_postfix(const Icu::Converter* converter) {
    return ucnv_getType(Native(Required(converter)));
}

std::int8_t APS5_VABI ucnv_isFixedWidth_nid_postfix(Icu::Converter* converter, std::int32_t* error) {
    return ucnv_isFixedWidth(Native(converter), &Required(error)) ? 1 : 0;
}

std::int8_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const Icu::Converter* converter) {
    return ucnv_isAmbiguous(Native(converter)) ? 1 : 0;
}

std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const Icu::Converter* converter, std::int32_t* error) {
    auto& status = Required(error);
    const auto ready = Ready(error);
    if (ready) Required(converter);
    const auto ccsid = ucnv_getCCSID(Native(converter), &status);
    RequireData(ready, error);
    return ccsid;
}

std::int32_t APS5_VABI ucnv_getPlatform_nid_postfix(const Icu::Converter* converter, std::int32_t* error) {
    auto& status = Required(error);
    if (U_SUCCESS(status)) Required(converter);
    return ucnv_getPlatform(Native(converter), &status);
}

void APS5_VABI ucnv_getStarters_nid_postfix(const Icu::Converter* converter, std::int8_t* starters, std::int32_t* error) {
    if (Ready(error)) Required(converter);
    ucnv_getStarters(Native(converter), reinterpret_cast<UBool*>(starters), Status(error));
}

void APS5_VABI ucnv_getSubstChars_nid_postfix(const Icu::Converter* converter, char* bytes, std::int8_t* length, std::int32_t* error) {
    auto& status = Required(error);
    if (U_FAILURE(status)) return;
    const auto* native = Native(Required(converter));
    Required(length);
    if (native->subCharLen > 0 && *length >= native->subCharLen) Required(bytes);
    ucnv_getSubstChars(native, bytes, length, &status);
}

void APS5_VABI ucnv_setSubstChars_nid_postfix(Icu::Converter* converter, const char* bytes, std::int8_t length, std::int32_t* error) {
    auto& status = Required(error);
    if (U_FAILURE(status)) return;
    auto* native = Native(Required(converter));
    const auto* data = native->sharedData->staticData;
    if (length >= data->minBytesPerChar && length <= data->maxBytesPerChar) Required(bytes);
    ucnv_setSubstChars(native, bytes, length, &status);
}

void APS5_VABI ucnv_setSubstString_nid_postfix(Icu::Converter* converter, const char16_t* text, std::int32_t length, std::int32_t* error) {
    ucnv_setSubstString(Native(converter), text, length, &Required(error));
}

void APS5_VABI ucnv_getInvalidChars_nid_postfix(const Icu::Converter* converter, char* bytes, std::int8_t* length, std::int32_t* error) {
    ucnv_getInvalidChars(Native(converter), bytes, length, Status(error));
}

void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const Icu::Converter* converter, char16_t* units, std::int8_t* length,
                                                 std::int32_t* error) {
    ucnv_getInvalidUChars(Native(converter), units, length, Status(error));
}

void APS5_VABI ucnv_setFallback_nid_postfix(Icu::Converter* converter, std::int8_t usesFallback) {
    ucnv_setFallback(Native(Required(converter)), usesFallback != 0);
}

std::int8_t APS5_VABI ucnv_usesFallback_nid_postfix(const Icu::Converter* converter) {
    return ucnv_usesFallback(Native(Required(converter))) ? 1 : 0;
}

std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const Icu::Converter* converter, std::int32_t* error) {
    return ucnv_fromUCountPending(Native(converter), Status(error));
}

std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const Icu::Converter* converter, std::int32_t* error) {
    return ucnv_toUCountPending(Native(converter), Status(error));
}

void APS5_VABI ucnv_fromUnicode_nid_postfix(Icu::Converter* converter, char** target, const char* targetLimit, const char16_t** source,
                                            const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* error) {
    ucnv_fromUnicode(Native(converter), target, targetLimit, source, sourceLimit, offsets, flush != 0, Status(error));
}

void APS5_VABI ucnv_toUnicode_nid_postfix(Icu::Converter* converter, char16_t** target, const char16_t* targetLimit, const char** source,
                                          const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* error) {
    ucnv_toUnicode(Native(converter), target, targetLimit, source, sourceLimit, offsets, flush != 0, Status(error));
}

std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(Icu::Converter* converter, char* destination, std::int32_t capacity, const char16_t* source,
                                                   std::int32_t length, std::int32_t* error) {
    return ucnv_fromUChars(Native(converter), destination, capacity, source, length, Status(error));
}

std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(Icu::Converter* converter, char16_t* destination, std::int32_t capacity, const char* source,
                                                 std::int32_t length, std::int32_t* error) {
    return ucnv_toUChars(Native(converter), destination, capacity, source, length, Status(error));
}

std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(Icu::Converter* converter, const char** source, const char* sourceLimit, std::int32_t* error) {
    return ucnv_getNextUChar(Native(converter), source, sourceLimit, Status(error));
}

void APS5_VABI ucnv_convertEx_nid_postfix(Icu::Converter* targetConverter, Icu::Converter* sourceConverter, char** target,
                                          const char* targetLimit, const char** source, const char* sourceLimit, char16_t* pivotStart,
                                          char16_t** pivotSource, char16_t** pivotTarget, const char16_t* pivotLimit, std::int8_t reset,
                                          std::int8_t flush, std::int32_t* error) {
    ucnv_convertEx(Native(targetConverter), Native(sourceConverter), target, targetLimit, source, sourceLimit, pivotStart, pivotSource,
                   pivotTarget, pivotLimit, reset != 0, flush != 0, Status(error));
}

std::int32_t APS5_VABI ucnv_convert_nid_postfix(const char* targetName, const char* sourceName, char* target, std::int32_t capacity,
                                                const char* source, std::int32_t length, std::int32_t* error) {
    const auto ready = Ready(error);
    if (ready && Converts(target, capacity, source, length)) {
        std::int32_t probe = U_ZERO_ERROR;
        ucnv_close(Open(sourceName, &probe));
        if (probe <= U_ZERO_ERROR) RequireName(targetName);
    }
    const auto converted = ucnv_convert(targetName, sourceName, target, capacity, source, length, Status(error));
    RequireData(ready, error);
    return converted;
}

std::int32_t APS5_VABI ucnv_toAlgorithmic_nid_postfix(std::int32_t type, Icu::Converter* converter, char* target, std::int32_t capacity,
                                                      const char* source, std::int32_t length, std::int32_t* error) {
    const auto ready = Ready(error);
    const auto converted =
        ucnv_toAlgorithmic(static_cast<UConverterType>(type), Native(converter), target, capacity, source, length, Status(error));
    RequireData(ready, error);
    return converted;
}

std::int32_t APS5_VABI ucnv_fromAlgorithmic_nid_postfix(Icu::Converter* converter, std::int32_t type, char* target, std::int32_t capacity,
                                                        const char* source, std::int32_t length, std::int32_t* error) {
    if (type < 0 || type >= UCNV_NUMBER_OF_SUPPORTED_CONVERTER_TYPES) Required(error);
    const auto ready = Ready(error);
    const auto converted =
        ucnv_fromAlgorithmic(Native(converter), static_cast<UConverterType>(type), target, capacity, source, length, Status(error));
    RequireData(ready, error);
    return converted;
}

}
