#pragma once
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uenum.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Icu {

static inline std::int32_t ReadEnumerationStatus(const std::int32_t* status, const char* caller) {
    if (!status) throw std::invalid_argument(std::string(caller) + ": null status");
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable status");
    return value;
}

static inline void WriteEnumerationStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (incoming == outgoing) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unwritable status");
}

static inline void WriteEnumerationLength(std::int32_t* length, std::int32_t value, const char* caller) {
    if (!length) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(length, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unwritable result length");
}

static inline UEnumeration* HostEnumeration(UEnumeration* enumeration) {
    return static_cast<UEnumeration*>(enumeration->context);
}

static inline void APS5_VABI CloseHostEnumeration(UEnumeration* enumeration) {
    uenum_close(HostEnumeration(enumeration));
    IcuReleaseMemory_nid_no_patch(enumeration);
}

static inline std::int32_t APS5_VABI CountHostEnumeration(UEnumeration* enumeration, std::int32_t* status) {
    const auto incoming = ReadEnumerationStatus(status, __func__);
    if (incoming > 0) return -1;
    auto local = static_cast<UErrorCode>(incoming);
    const auto result = uenum_count(HostEnumeration(enumeration), &local);
    WriteEnumerationStatus(status, incoming, local, __func__);
    return result;
}

static inline const char16_t* APS5_VABI UnicodeNextHostEnumeration(UEnumeration* enumeration, std::int32_t* resultLength,
                                                            std::int32_t* status) {
    const auto incoming = ReadEnumerationStatus(status, __func__);
    if (incoming > 0) return nullptr;
    auto local = static_cast<UErrorCode>(incoming);
    std::int32_t length = 0;
    const auto* result = uenum_unext(HostEnumeration(enumeration), &length, &local);
    WriteEnumerationStatus(status, incoming, local, __func__);
    WriteEnumerationLength(resultLength, length, __func__);
    return result;
}

static inline const char* APS5_VABI NextHostEnumeration(UEnumeration* enumeration, std::int32_t* resultLength,
                                                 std::int32_t* status) {
    const auto incoming = ReadEnumerationStatus(status, __func__);
    if (incoming > 0) return nullptr;
    auto local = static_cast<UErrorCode>(incoming);
    std::int32_t length = 0;
    const auto* result = uenum_next(HostEnumeration(enumeration), &length, &local);
    WriteEnumerationStatus(status, incoming, local, __func__);
    WriteEnumerationLength(resultLength, length, __func__);
    return result;
}

static inline void APS5_VABI ResetHostEnumeration(UEnumeration* enumeration, std::int32_t* status) {
    const auto incoming = ReadEnumerationStatus(status, __func__);
    if (incoming > 0) return;
    auto local = static_cast<UErrorCode>(incoming);
    uenum_reset(HostEnumeration(enumeration), &local);
    WriteEnumerationStatus(status, incoming, local, __func__);
}

template<class TOpen>
static UEnumeration* OpenHostEnumeration(std::int32_t* status, TOpen open, const char* caller) {
    const auto incoming = ReadEnumerationStatus(status, caller);
    if (incoming > 0) return nullptr;
    auto local = static_cast<UErrorCode>(incoming);
    UEnumeration* host = open(&local);
    try {
        WriteEnumerationStatus(status, incoming, local, caller);
    } catch (...) {
        uenum_close(host);
        throw;
    }
    if (!host) return nullptr;
    auto* enumeration = static_cast<UEnumeration*>(uprv_malloc_nid_postfix(sizeof(UEnumeration)));
    if (!enumeration) {
        uenum_close(host);
        const auto memoryError = static_cast<std::int32_t>(U_MEMORY_ALLOCATION_ERROR);
        WriteEnumerationStatus(status, incoming, memoryError, caller);
        return nullptr;
    }
    *enumeration = {nullptr, host, CloseHostEnumeration, CountHostEnumeration, UnicodeNextHostEnumeration,
                    NextHostEnumeration, ResetHostEnumeration};
    return enumeration;
}

}
