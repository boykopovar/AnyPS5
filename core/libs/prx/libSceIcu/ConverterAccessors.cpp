// Derived from icu4c/source/common/ucnv.c@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the converter
// accessors below forward to the same host ICU entry points the rest of this library uses.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucnv.c
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1996-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/ConverterAccessors.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <string>

namespace {
constexpr std::size_t ErrorBufferLength = 32;
}
#include <cerrno>
#include <cstdint>

namespace {

constexpr std::int32_t IllegalArgument = 1;

bool ReadStatus(const std::int32_t* status, UErrorCode& local, const char* caller) {
    if (!status) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        return false;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&local, status, sizeof(local));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return true;
}

void WriteStatus(std::int32_t* status, UErrorCode local, const char* caller) {
    if (!status) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &local, sizeof(local));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

UConverter* HostConverter(const UConverter* converter) {
    return const_cast<UConverter*>(converter);
}

extern "C" {

std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMaxCharSize(HostConverter(converter));
}

std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMinCharSize(HostConverter(converter));
}

std::int32_t APS5_VABI ucnv_getType_nid_postfix(const UConverter* converter) {
    return static_cast<std::int32_t>(ucnv_getType(HostConverter(converter)));
}

std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return -1;
    const std::int32_t result = static_cast<std::int32_t>(ucnv_getCCSID(HostConverter(converter), &local));
    WriteStatus(status, local, __func__);
    return result;
}

std::int8_t APS5_VABI ucnv_getPlatform_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return static_cast<std::int8_t>(UCNV_UNKNOWN);
    const std::int8_t result = static_cast<std::int8_t>(ucnv_getPlatform(HostConverter(converter), &local));
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const UConverter* converter) {
    return ucnv_isAmbiguous(HostConverter(converter)) ? 1 : 0;
}

std::int32_t APS5_VABI ucnv_isFixedWidth_nid_postfix(UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    const std::int32_t result = ucnv_isFixedWidth(converter, &local) ? 1 : 0;
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_usesFallback_nid_postfix(const UConverter* converter) {
    return ucnv_usesFallback(HostConverter(converter)) ? 1 : 0;
}

std::int32_t APS5_VABI ucnv_countAvailable_nid_postfix() {
    return ucnv_countAvailable();
}

const char* APS5_VABI ucnv_getAvailableName_nid_postfix(std::int32_t number) {
    return ucnv_getAvailableName(number);
}

std::int32_t APS5_VABI ucnv_countStandards_nid_postfix() {
    return static_cast<std::int32_t>(ucnv_countStandards());
}

const char* APS5_VABI ucnv_getStandard_nid_postfix(std::int32_t number, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    const char* result = ucnv_getStandard(static_cast<std::uint16_t>(number), &local);
    WriteStatus(status, local, __func__);
    return result;
}

const char* APS5_VABI ucnv_getStandardName_nid_postfix(const char* name, const char* standard,
    std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    const char* result = ucnv_getStandardName(name, standard, &local);
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_countAliases_nid_postfix(const char* alias, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return 0;
    const std::int32_t result = static_cast<std::int32_t>(ucnv_countAliases(alias, &local));
    WriteStatus(status, local, __func__);
    return result;
}

const char* APS5_VABI ucnv_getAlias_nid_postfix(const char* alias, std::uint16_t number, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    const char* result = ucnv_getAlias(alias, number, &local);
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return -1;
    const std::int32_t result = ucnv_toUCountPending(HostConverter(converter), &local);
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return -1;
    const std::int32_t result = ucnv_fromUCountPending(HostConverter(converter), &local);
    WriteStatus(status, local, __func__);
    return result;
}

std::int32_t APS5_VABI ucnv_flushCache_nid_postfix() {
    return ucnv_flushCache() ? 1 : 0;
}

UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    std::string converterName;
    if (name == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return nullptr;
    }
    for (std::size_t capacity = 64; capacity <= 4096; capacity *= 2) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(name, static_cast<int>(capacity), converterName);
        errno = savedErrno;
        if (error == 0) break;
        if (error != 63) {
            local = U_ILLEGAL_ARGUMENT_ERROR;
            WriteStatus(status, local, __func__);
            return nullptr;
        }
    }
    UConverter* converter = ucnv_open(converterName.c_str(), &local);
    WriteStatus(status, local, __func__);
    return converter;
}

void APS5_VABI ucnv_close_nid_postfix(UConverter* converter) {
    if (converter != nullptr) ucnv_close(converter);
}

const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    const char* name = ucnv_getName(HostConverter(converter), &local);
    WriteStatus(status, local, __func__);
    return name;
}

void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter) {
    if (converter != nullptr) ucnv_reset(converter);
}

void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int32_t usesFallback) {
    if (converter != nullptr) ucnv_setFallback(converter, usesFallback != 0 ? UBool(1) : UBool(0));
}

void APS5_VABI ucnv_getInvalidChars_nid_postfix(const UConverter* converter, char* errorBytes,
    std::int8_t* errorLength, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (errorBytes == nullptr || errorLength == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    std::int8_t length = 0;
    std::uint8_t bytes[ErrorBufferLength] = {};
    ucnv_getInvalidChars(HostConverter(converter), reinterpret_cast<char*>(bytes), &length, &local);
    if (local <= 0) {
        const int savedErrno = errno;
        GuestCopyOut_nid_no_patch(errorBytes, bytes, sizeof(bytes));
        GuestCopyOut_nid_no_patch(errorLength, &length, sizeof(length));
        errno = savedErrno;
    }
    WriteStatus(status, local, __func__);
}

void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const UConverter* converter, char16_t* errorChars,
    std::int8_t* errorLength, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (errorChars == nullptr || errorLength == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    std::int8_t length = 0;
    char16_t chars[ErrorBufferLength] = {};
    ucnv_getInvalidUChars(HostConverter(converter), reinterpret_cast<UChar*>(chars), &length, &local);
    if (local <= 0) {
        const int savedErrno = errno;
        GuestCopyOut_nid_no_patch(errorChars, chars, sizeof(chars));
        GuestCopyOut_nid_no_patch(errorLength, &length, sizeof(length));
        errno = savedErrno;
    }
    WriteStatus(status, local, __func__);
}


void APS5_VABI ucnv_setSubstString_nid_postfix(UConverter* converter, const char16_t* substitution, std::int32_t length,
    std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (substitution == nullptr || length < 0) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    std::vector<char16_t> chars(static_cast<std::size_t>(length) + 1, 0);
    const int savedErrno = errno;
    GuestCopyIn_nid_no_patch(chars.data(), substitution, static_cast<std::size_t>(length) * sizeof(char16_t));
    errno = savedErrno;
    ucnv_setSubstString(converter, reinterpret_cast<const UChar*>(chars.data()), length, &local);
    WriteStatus(status, local, __func__);
}

void APS5_VABI ucnv_getStarters_nid_postfix(const UConverter* converter, std::int8_t* starters, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (starters == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    std::uint8_t values[256] = {};
    ucnv_getStarters(HostConverter(converter), reinterpret_cast<UBool*>(values), &local);
    if (local <= 0) {
        const int savedErrno = errno;
        for (std::size_t index = 0; index < 256; ++index) {
            const std::int8_t value = values[index] != 0 ? 1 : 0;
            GuestCopyOut_nid_no_patch(starters + index, &value, sizeof(value));
        }
        errno = savedErrno;
    }
    WriteStatus(status, local, __func__);
}


}
}

extern "C" std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination,
    std::int32_t capacity, const char16_t* source, std::int32_t length, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (status == nullptr) return -1;
    {
        const int saved = errno;
        GuestCopyIn_nid_no_patch(&local, status, sizeof(local));
        errno = saved;
    }
    if (local > 0) return -1;
    if (converter == nullptr || source == nullptr || capacity < 0 || length < -1 ||
        (capacity > 0 && destination == nullptr)) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        const int saved = errno;
        GuestCopyOut_nid_no_patch(status, &local, sizeof(local));
        errno = saved;
        return -1;
    }

    std::int32_t sourceLength = length;
    if (sourceLength < 0) {
        sourceLength = 0;
        bool terminated = false;
        while (!terminated) {
            char16_t probe[64] = {};
            const int saved = errno;
            GuestCopyIn_nid_no_patch(probe, source + sourceLength, sizeof(probe));
            errno = saved;
            for (std::size_t index = 0; index < 64; ++index) {
                if (probe[index] == 0) {
                    terminated = true;
                    break;
                }
                ++sourceLength;
            }
        }
    }

    std::vector<char16_t> wide(static_cast<std::size_t>(sourceLength) + 1, 0);
    std::vector<char> narrow(static_cast<std::size_t>(capacity > 0 ? capacity : 1), 0);
    const std::int32_t copiedSource = sourceLength;
    char* narrowData = narrow.data();
    {
        const int saved = errno;
        GuestCopyIn_nid_no_patch(wide.data(), source, static_cast<std::size_t>(copiedSource) * sizeof(char16_t));
        errno = saved;
    }

    const std::int32_t produced = ucnv_fromUChars(converter, narrowData, capacity,
        reinterpret_cast<const UChar*>(wide.data()), copiedSource, &local);
    const std::int32_t written = local == U_BUFFER_OVERFLOW_ERROR ? capacity : produced;
    if (written > 0 && capacity > 0) {
        const int saved = errno;
        GuestCopyOut_nid_no_patch(destination, narrowData, static_cast<std::size_t>(written));
        errno = saved;
    }
    {
        const int saved = errno;
        GuestCopyOut_nid_no_patch(status, &local, sizeof(local));
        errno = saved;
    }
    return produced;
}

void APS5_VABI ucnv_getFromUCallBack_nid_postfix(const UConverter* converter, void** action, void** context) {
    UConverterFromUCallback callback = nullptr;
    const void* callbackContext = nullptr;
    ucnv_getFromUCallBack(HostConverter(converter), &callback, &callbackContext);
    const int saved = errno;
    if (action) {
        void* stored = reinterpret_cast<void*>(callback);
        GuestCopyOut_nid_no_patch(action, &stored, sizeof(stored));
    }
    if (context) {
        void* stored = const_cast<void*>(callbackContext);
        GuestCopyOut_nid_no_patch(context, &stored, sizeof(stored));
    }
    errno = saved;
}

void APS5_VABI ucnv_getToUCallBack_nid_postfix(const UConverter* converter, void** action, void** context) {
    UConverterToUCallback callback = nullptr;
    const void* callbackContext = nullptr;
    ucnv_getToUCallBack(HostConverter(converter), &callback, &callbackContext);
    const int saved = errno;
    if (action) {
        void* stored = reinterpret_cast<void*>(callback);
        GuestCopyOut_nid_no_patch(action, &stored, sizeof(stored));
    }
    if (context) {
        void* stored = const_cast<void*>(callbackContext);
        GuestCopyOut_nid_no_patch(context, &stored, sizeof(stored));
    }
    errno = saved;
}

extern "C" std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(UConverter* converter, const char** source,
    const char* sourceLimit, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return -1;
    if (converter == nullptr || source == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return -1;
    }

    const char* guestSource = nullptr;
    {
        const int saved = errno;
        GuestCopyIn_nid_no_patch(&guestSource, source, sizeof(guestSource));
        errno = saved;
    }
    if (guestSource == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return -1;
    }

    constexpr std::size_t ChunkLimit = 64;
    std::size_t available = ChunkLimit;
    if (sourceLimit != nullptr) {
        const std::size_t remaining = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(sourceLimit)) -
            reinterpret_cast<std::uintptr_t>(guestSource);
        if (remaining < available) available = remaining;
    }

    char chunk[ChunkLimit] = {};
    if (available > 0) {
        const int saved = errno;
        GuestCopyIn_nid_no_patch(chunk, guestSource, available);
        errno = saved;
    }

    const char* cursor = chunk;
    const char* limit = chunk + available;
    const std::int32_t codePoint = ucnv_getNextUChar(converter, &cursor, limit, &local);
    const std::ptrdiff_t consumed = cursor - chunk;
    if (local <= 0 && consumed >= 0) {
        const char* advanced = guestSource + consumed;
        const int saved = errno;
        GuestCopyOut_nid_no_patch(source, &advanced, sizeof(advanced));
        errno = saved;
    }
    WriteStatus(status, local, __func__);
    return codePoint;
}
