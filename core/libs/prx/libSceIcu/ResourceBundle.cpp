// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t Icu68ResourceBundleSize = 200;
constexpr std::size_t Icu68ResourceDataEntryOffset = 8;
constexpr std::size_t Icu68ResourceOffset = 172;
constexpr std::uint32_t IntegerType = 7;
constexpr std::uint32_t TypeShift = 28;
constexpr std::uint32_t ValueMask = 0x0fffffff;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t ResourceTypeMismatch = 17;

bool CopyIn(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return copied;
}

void CopyOut(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

void APS5_VABI ures_close(UResourceBundle* resB) {
    if (!resB) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ures_initStackObject_nid_postfix(UResourceBundle* resB) {
    std::memset(resB, 0, Icu68ResourceBundleSize);
}

const char* APS5_VABI ures_getLocaleInternal_nid_postfix(const UResourceBundle* bundle, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!bundle) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const auto address = reinterpret_cast<std::uintptr_t>(bundle);
    if (address > UINTPTR_MAX - Icu68ResourceDataEntryOffset) NotImplemented_nid_no_patch(__func__);
    const void* entry;
    CopyIn(&entry, reinterpret_cast<const void*>(address + Icu68ResourceDataEntryOffset), sizeof(entry), savedErrno,
           __func__);
    const char* name;
    CopyIn(&name, entry, sizeof(name), savedErrno, __func__);
    return name;
}

const char* APS5_VABI ures_getLocale_nid_postfix(const UResourceBundle* bundle, std::int32_t* status) {
    return ures_getLocaleInternal_nid_postfix(bundle, status);
}

std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resB, std::int32_t* status) {
    constexpr std::uint32_t failure = UINT32_MAX;
    if (!status) return failure;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return failure;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }

    std::uint32_t resource;
    const auto* resourceAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset;
    CopyIn(&resource, resourceAddress, sizeof(resource), savedErrno, __func__);
    if ((resource >> TypeShift) != IntegerType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }
    return resource & ValueMask;
}

}
