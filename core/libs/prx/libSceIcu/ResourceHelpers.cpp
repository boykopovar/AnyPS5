// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1999-2016, International Business Machines Corporation
*               and others. All Rights Reserved.
*******************************************************************************
*   file name:  uresdata.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999dec08
*   created by: Markus W. Scherer
* Modification History:
*
*   Date        Name        Description
*   06/20/2000  helena      OS/400 port changes; mostly typecast.
*   06/24/02    weiv        Added support for resource sharing
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresdata.cpp

#include "prx/libSceIcu/ResourceHelpers.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::uint32_t TypeShift = 28;
constexpr std::uint32_t OffsetMask = 0x0fffffff;
constexpr std::size_t Icu68RootOffset = 8;
constexpr std::size_t Icu68UnitOffset = 16;

std::uintptr_t ReadPointerField(const void* resourceData, std::size_t fieldOffset, const char* caller) {
    if (resourceData == nullptr) NotImplemented_nid_no_patch(caller);
    std::uintptr_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value,
        static_cast<const std::uint8_t*>(resourceData) + fieldOffset, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
T ReadValue(std::uintptr_t address, const char* caller) {
    T value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return value;
}

}

extern "C" std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const void* resourceData,
    std::uint32_t resource) {
    const std::uint32_t offset = resource & OffsetMask;
    switch (resource >> TypeShift) {
    case 0:
    case 1:
    case 3:
    case 6:
    case 7:
    case 14:
        return 1;
    case 4:
    case 8: {
        if (offset == 0) return 0;
        const std::uintptr_t root = ReadPointerField(resourceData, Icu68RootOffset, __func__);
        return ReadValue<std::int32_t>(root + static_cast<std::uintptr_t>(offset) * sizeof(std::int32_t), __func__);
    }
    case 2: {
        if (offset == 0) return 0;
        const std::uintptr_t root = ReadPointerField(resourceData, Icu68RootOffset, __func__);
        return ReadValue<std::uint16_t>(root + static_cast<std::uintptr_t>(offset) * sizeof(std::int32_t), __func__);
    }
    case 5:
    case 9: {
        const std::uintptr_t units = ReadPointerField(resourceData, Icu68UnitOffset, __func__);
        return ReadValue<std::uint16_t>(units + static_cast<std::uintptr_t>(offset) * sizeof(std::uint16_t), __func__);
    }
    default:
        return 0;
    }
}
