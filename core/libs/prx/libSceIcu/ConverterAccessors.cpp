// Derived from icu4c/source/common/ucnv.c@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the converter
// entry points below forward to the same host ICU implementation the rest of this library links.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucnv.c
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1996-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/ConverterAccessors.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr std::size_t ChunkLength = 4096;

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

template <typename T>
bool ReadPointer(const T* address, T& value, const char* caller) {
    if (!address) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, address, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return true;
}

template <typename T>
void WritePointer(T* address, T value, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(address, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes == 0) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    if (bytes == 0) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return nullptr;
    if (name == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return nullptr;
    }
    std::string converterName;
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

void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** target, const char* targetLimit,
    const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush,
    std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (U_FAILURE(local)) return;
    if (offsets != nullptr || converter == nullptr || target == nullptr || targetLimit == nullptr ||
        source == nullptr || sourceLimit == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    char* guestTarget = nullptr;
    const char16_t* guestSource = nullptr;
    if (!ReadPointer(target, guestTarget, __func__) || !ReadPointer(source, guestSource, __func__) ||
        guestTarget == nullptr || guestSource == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    const std::uintptr_t targetAddress = reinterpret_cast<std::uintptr_t>(guestTarget);
    const std::uintptr_t targetLimitAddress = reinterpret_cast<std::uintptr_t>(targetLimit);
    const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(guestSource);
    const std::uintptr_t sourceLimitAddress = reinterpret_cast<std::uintptr_t>(sourceLimit);
    if (targetLimitAddress < targetAddress || sourceLimitAddress < sourceAddress) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    const std::size_t targetCapacity = targetLimitAddress - targetAddress;
    const std::size_t sourceUnits = (sourceLimitAddress - sourceAddress) / sizeof(char16_t);

    char narrow[ChunkLength] = {};
    char16_t wide[ChunkLength] = {};
    char* targetCursor = guestTarget;
    const char16_t* sourceCursor = guestSource;
    std::size_t remainingTarget = targetCapacity;
    std::size_t remainingSource = sourceUnits;
    for (;;) {
        const std::size_t chunkUnits = remainingSource < ChunkLength ? remainingSource : ChunkLength;
        const std::size_t chunkBytes = remainingTarget < ChunkLength ? remainingTarget : ChunkLength;
        const bool lastChunk = chunkUnits == remainingSource;
        CopyIn(wide, sourceCursor, chunkUnits * sizeof(char16_t), __func__);
        char* localTarget = narrow;
        const UChar* localSource = reinterpret_cast<const UChar*>(wide);
        ucnv_fromUnicode(converter, &localTarget, narrow + chunkBytes, &localSource,
            reinterpret_cast<const UChar*>(wide) + chunkUnits, nullptr,
            lastChunk && flush != 0 ? UBool(1) : UBool(0), &local);
        const std::size_t producedBytes = static_cast<std::size_t>(localTarget - narrow);
        const std::size_t consumedUnits =
            static_cast<std::size_t>(localSource - reinterpret_cast<const UChar*>(wide));
        CopyOut(targetCursor, narrow, producedBytes, __func__);
        targetCursor += producedBytes;
        sourceCursor += consumedUnits;
        remainingTarget -= producedBytes;
        remainingSource -= consumedUnits;
        if (U_FAILURE(local)) break;
        if (lastChunk && consumedUnits == chunkUnits) break;
        if (producedBytes == 0 && consumedUnits == 0) break;
    }
    WritePointer(target, targetCursor, __func__);
    WritePointer(source, sourceCursor, __func__);
    WriteStatus(status, local, __func__);
}

void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** target, const char16_t* targetLimit,
    const char** source, const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status) {
    UErrorCode local = U_ZERO_ERROR;
    if (!ReadStatus(status, local, __func__)) return;
    if (U_FAILURE(local)) return;
    if (offsets != nullptr || converter == nullptr || target == nullptr || targetLimit == nullptr ||
        source == nullptr || sourceLimit == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    char16_t* guestTarget = nullptr;
    const char* guestSource = nullptr;
    if (!ReadPointer(target, guestTarget, __func__) || !ReadPointer(source, guestSource, __func__) ||
        guestTarget == nullptr || guestSource == nullptr) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    const std::uintptr_t targetAddress = reinterpret_cast<std::uintptr_t>(guestTarget);
    const std::uintptr_t targetLimitAddress = reinterpret_cast<std::uintptr_t>(targetLimit);
    const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(guestSource);
    const std::uintptr_t sourceLimitAddress = reinterpret_cast<std::uintptr_t>(sourceLimit);
    if (targetLimitAddress < targetAddress || sourceLimitAddress < sourceAddress) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        WriteStatus(status, local, __func__);
        return;
    }
    const std::size_t targetUnits = (targetLimitAddress - targetAddress) / sizeof(char16_t);
    const std::size_t sourceBytes = sourceLimitAddress - sourceAddress;

    char narrow[ChunkLength] = {};
    char16_t wide[ChunkLength] = {};
    char16_t* targetCursor = guestTarget;
    const char* sourceCursor = guestSource;
    std::size_t remainingTarget = targetUnits;
    std::size_t remainingSource = sourceBytes;
    for (;;) {
        const std::size_t chunkUnits = remainingTarget < ChunkLength ? remainingTarget : ChunkLength;
        const std::size_t chunkBytes = remainingSource < ChunkLength ? remainingSource : ChunkLength;
        const bool lastChunk = chunkBytes == remainingSource;
        CopyIn(narrow, sourceCursor, chunkBytes, __func__);
        UChar* localTarget = reinterpret_cast<UChar*>(wide);
        const char* localSource = narrow;
        ucnv_toUnicode(converter, &localTarget, reinterpret_cast<const UChar*>(wide) + chunkUnits, &localSource,
            narrow + chunkBytes, nullptr, lastChunk && flush != 0 ? UBool(1) : UBool(0), &local);
        const std::size_t producedUnits =
            static_cast<std::size_t>(localTarget - reinterpret_cast<const UChar*>(wide));
        const std::size_t consumedBytes = static_cast<std::size_t>(localSource - narrow);
        CopyOut(targetCursor, wide, producedUnits * sizeof(char16_t), __func__);
        targetCursor += producedUnits;
        sourceCursor += consumedBytes;
        remainingTarget -= producedUnits;
        remainingSource -= consumedBytes;
        if (U_FAILURE(local)) break;
        if (lastChunk && consumedBytes == chunkBytes) break;
        if (producedUnits == 0 && consumedBytes == 0) break;
    }
    WritePointer(target, targetCursor, __func__);
    WritePointer(source, sourceCursor, __func__);
    WriteStatus(status, local, __func__);
}

}
