// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udatamem.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucmndata.cpp

#include "prx/libSceIcu/include/DataMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t HeaderPointerOffset = 8;
constexpr std::size_t TotalLengthOffset = 48;
constexpr std::size_t EndiannessOffset = 8;
static_assert(sizeof(void*) == 8);

const void* AtOffset(const void* base, std::size_t offset) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address) {
        throw std::invalid_argument("ICU data memory: address overflow");
    }
    return reinterpret_cast<const void*>(address + offset);
}

template<class T>
T ReadField(const void* base, std::size_t offset) {
    T value;
    if (!GuestCopyIn_nid_no_patch(&value, AtOffset(base, offset), sizeof(value))) {
        throw std::invalid_argument("ICU data memory: unreadable field");
    }
    return value;
}

const void* Header(const Icu::DataMemory* data) {
    return data ? ReadField<const void*>(data, HeaderPointerOffset) : nullptr;
}

std::uint16_t HeaderSize(const void* header) {
    const auto bigEndian = ReadField<std::uint8_t>(header, EndiannessOffset);
    const auto size = ReadField<std::uint16_t>(header, 0);
    return bigEndian ? static_cast<std::uint16_t>((size << 8) | (size >> 8)) : size;
}

}

extern "C" const void* APS5_VABI udata_getRawMemory_nid_postfix(const Icu::DataMemory* data) {
    return Header(data);
}

extern "C" const void* APS5_VABI udata_getMemory_nid_postfix(Icu::DataMemory* data) {
    const auto* header = Header(data);
    return header ? AtOffset(header, HeaderSize(header)) : nullptr;
}

extern "C" std::int32_t APS5_VABI udata_getLength_nid_postfix(const Icu::DataMemory* data) {
    const auto* header = Header(data);
    if (!header) return -1;
    const auto total = ReadField<std::int32_t>(data, TotalLengthOffset);
    return total < 0 ? -1 : total - HeaderSize(header);
}
