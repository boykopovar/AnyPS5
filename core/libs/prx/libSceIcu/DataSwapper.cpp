// Derived from icu4c/source/common/udataswp.cpp and uinvchar.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2).
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udataswp.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.cpp
//
// The notice of udataswp.cpp, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.
//
// The notice of uinvchar.cpp, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2010, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include "ucmndata.h"
#include "udataswp.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

static_assert(sizeof(Icu::DataSwapper) == sizeof(UDataSwapper));
static_assert(alignof(Icu::DataSwapper) == alignof(UDataSwapper));
static_assert(offsetof(Icu::DataSwapper, inIsBigEndian) == offsetof(UDataSwapper, inIsBigEndian));
static_assert(offsetof(Icu::DataSwapper, inCharset) == offsetof(UDataSwapper, inCharset));
static_assert(offsetof(Icu::DataSwapper, outIsBigEndian) == offsetof(UDataSwapper, outIsBigEndian));
static_assert(offsetof(Icu::DataSwapper, outCharset) == offsetof(UDataSwapper, outCharset));
static_assert(offsetof(Icu::DataSwapper, readUInt16) == offsetof(UDataSwapper, readUInt16));
static_assert(offsetof(Icu::DataSwapper, readUInt32) == offsetof(UDataSwapper, readUInt32));
static_assert(offsetof(Icu::DataSwapper, compareInvChars) == offsetof(UDataSwapper, compareInvChars));
static_assert(offsetof(Icu::DataSwapper, writeUInt16) == offsetof(UDataSwapper, writeUInt16));
static_assert(offsetof(Icu::DataSwapper, writeUInt32) == offsetof(UDataSwapper, writeUInt32));
static_assert(offsetof(Icu::DataSwapper, swapArray16) == offsetof(UDataSwapper, swapArray16));
static_assert(offsetof(Icu::DataSwapper, swapArray32) == offsetof(UDataSwapper, swapArray32));
static_assert(offsetof(Icu::DataSwapper, swapArray64) == offsetof(UDataSwapper, swapArray64));
static_assert(offsetof(Icu::DataSwapper, swapInvChars) == offsetof(UDataSwapper, swapInvChars));
static_assert(offsetof(Icu::DataSwapper, printError) == offsetof(UDataSwapper, printError));
static_assert(offsetof(Icu::DataSwapper, printErrorContext) == offsetof(UDataSwapper, printErrorContext));
static_assert(sizeof(DataHeader) == 24);
static_assert(sizeof(UDataInfo) == 20);

namespace {

constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int8_t GuestIsBigEndian = 0;
constexpr std::uint8_t AsciiFamily = 0;
constexpr std::uint8_t EbcdicFamily = 1;
constexpr std::uint8_t MagicFirst = 0xda;
constexpr std::uint8_t MagicSecond = 0x27;

constexpr std::uint8_t AsciiFromEbcdicTable[256] = {
    0x00, 0x01, 0x02, 0x03, 0x00, 0x09, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x00, 0x0a, 0x08, 0x00, 0x18, 0x19, 0x00, 0x00, 0x1c, 0x1d, 0x1e, 0x1f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x17, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x06, 0x07,
    0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x14, 0x15, 0x00, 0x1a,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,
    0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
    0x2d, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00,
    0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x5d, 0x00, 0x5d, 0x00, 0x00,
    0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5c, 0x00, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr std::uint8_t EbcdicFromAsciiTable[256] = {
    0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26, 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
    0x40, 0x00, 0x7f, 0x00, 0x00, 0x6c, 0x50, 0x7d, 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
    0x00, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0x00, 0x00, 0x00, 0x00, 0x6d,
    0x00, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x07,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

constexpr std::uint32_t InvariantChars[4] = {0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};

template<class T>
T Load(const void* source) {
    T value;
    std::memcpy(&value, source, sizeof(T));
    return value;
}

template<class T>
void Store(void* destination, T value) {
    std::memcpy(destination, &value, sizeof(T));
}

bool IsInvariant(std::uint32_t character) {
    return character <= 0x7f && (InvariantChars[character >> 5] & (std::uint32_t{1} << (character & 0x1f))) != 0;
}

bool Failed(const std::int32_t* status) {
    return !status || *status > 0;
}

std::uint16_t SwapUInt16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t SwapUInt32(std::uint32_t value) {
    return (value << 24) | ((value << 8) & 0xff0000) | ((value >> 8) & 0xff00) | (value >> 24);
}

std::uint64_t SwapUInt64(std::uint64_t value) {
    return (value << 56) | ((value & 0xff00) << 40) | ((value & 0xff0000) << 24) | ((value & 0xff000000) << 8) |
           ((value >> 8) & 0xff000000) | ((value >> 24) & 0xff0000) | ((value >> 40) & 0xff00) | (value >> 56);
}

bool ArrayArguments(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length, const void* outData,
                    std::int32_t unitMask, std::int32_t* status) {
    if (Failed(status)) return false;
    if (!swapper || !inData || length < 0 || (length & unitMask) != 0 || !outData) {
        *status = Icu::IllegalArgument;
        return false;
    }
    return true;
}

bool CharArguments(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length, const void* outData,
                   std::int32_t* status) {
    if (Failed(status)) return false;
    if (!swapper || !inData || length < 0 || (length > 0 && !outData)) {
        *status = Icu::IllegalArgument;
        return false;
    }
    return true;
}

std::int32_t APS5_VABI SwapArray16(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    if (!ArrayArguments(swapper, inData, length, outData, 1, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    auto* destination = static_cast<std::uint8_t*>(outData);
    for (std::int32_t count = length / 2; count > 0; --count, source += 2, destination += 2) {
        Store(destination, SwapUInt16(Load<std::uint16_t>(source)));
    }
    return length;
}

std::int32_t APS5_VABI SwapArray32(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    if (!ArrayArguments(swapper, inData, length, outData, 3, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    auto* destination = static_cast<std::uint8_t*>(outData);
    for (std::int32_t count = length / 4; count > 0; --count, source += 4, destination += 4) {
        Store(destination, SwapUInt32(Load<std::uint32_t>(source)));
    }
    return length;
}

std::int32_t APS5_VABI SwapArray64(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    if (!ArrayArguments(swapper, inData, length, outData, 7, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    auto* destination = static_cast<std::uint8_t*>(outData);
    for (std::int32_t count = length / 8; count > 0; --count, source += 8, destination += 8) {
        Store(destination, SwapUInt64(Load<std::uint64_t>(source)));
    }
    return length;
}

std::int32_t CopyArray(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length, void* outData,
                       std::int32_t unitMask, std::int32_t* status) {
    if (!ArrayArguments(swapper, inData, length, outData, unitMask, status)) return 0;
    if (length > 0 && inData != outData) std::memcpy(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI CopyArray16(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    return CopyArray(swapper, inData, length, outData, 1, status);
}

std::int32_t APS5_VABI CopyArray32(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    return CopyArray(swapper, inData, length, outData, 3, status);
}

std::int32_t APS5_VABI CopyArray64(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                   void* outData, std::int32_t* status) {
    return CopyArray(swapper, inData, length, outData, 7, status);
}

std::uint16_t APS5_VABI ReadSwapUInt16(std::uint16_t value) {
    return SwapUInt16(value);
}

std::uint16_t APS5_VABI ReadDirectUInt16(std::uint16_t value) {
    return value;
}

std::uint32_t APS5_VABI ReadSwapUInt32(std::uint32_t value) {
    return SwapUInt32(value);
}

std::uint32_t APS5_VABI ReadDirectUInt32(std::uint32_t value) {
    return value;
}

void APS5_VABI WriteSwapUInt16(std::uint16_t* destination, std::uint16_t value) {
    Store(destination, SwapUInt16(value));
}

void APS5_VABI WriteDirectUInt16(std::uint16_t* destination, std::uint16_t value) {
    Store(destination, value);
}

void APS5_VABI WriteSwapUInt32(std::uint32_t* destination, std::uint32_t value) {
    Store(destination, SwapUInt32(value));
}

void APS5_VABI WriteDirectUInt32(std::uint32_t* destination, std::uint32_t value) {
    Store(destination, value);
}

std::int32_t APS5_VABI EbcdicFromAscii(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                       void* outData, std::int32_t* status) {
    if (!CharArguments(swapper, inData, length, outData, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    auto* destination = static_cast<std::uint8_t*>(outData);
    for (std::int32_t count = length; count > 0; --count) {
        const std::uint8_t character = *source++;
        if (!IsInvariant(character)) {
            udata_printError_nid_postfix(swapper,
                "uprv_ebcdicFromAscii() string[%d] contains a variant character in position %d\n", length,
                length - count);
            *status = Icu::InvalidCharacter;
            return 0;
        }
        *destination++ = EbcdicFromAsciiTable[character];
    }
    return length;
}

std::int32_t APS5_VABI CopyAscii(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                 void* outData, std::int32_t* status) {
    if (!CharArguments(swapper, inData, length, outData, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    for (std::int32_t count = length; count > 0; --count) {
        if (!IsInvariant(*source++)) {
            udata_printError_nid_postfix(swapper,
                "uprv_copyFromAscii() string[%d] contains a variant character in position %d\n", length,
                length - count);
            *status = Icu::InvalidCharacter;
            return 0;
        }
    }
    if (length > 0 && inData != outData) std::memcpy(outData, inData, static_cast<std::size_t>(length));
    return length;
}

bool EbcdicInvariant(std::uint8_t& character) {
    if (character == 0) return true;
    character = AsciiFromEbcdicTable[character];
    return character != 0 && IsInvariant(character);
}

std::int32_t APS5_VABI AsciiFromEbcdic(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                       void* outData, std::int32_t* status) {
    if (!CharArguments(swapper, inData, length, outData, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    auto* destination = static_cast<std::uint8_t*>(outData);
    for (std::int32_t count = length; count > 0; --count) {
        std::uint8_t character = *source++;
        if (!EbcdicInvariant(character)) {
            udata_printError_nid_postfix(swapper,
                "uprv_asciiFromEbcdic() string[%d] contains a variant character in position %d\n", length,
                length - count);
            *status = Icu::InvalidCharacter;
            return 0;
        }
        *destination++ = character;
    }
    return length;
}

std::int32_t APS5_VABI CopyEbcdic(const Icu::DataSwapper* swapper, const void* inData, std::int32_t length,
                                  void* outData, std::int32_t* status) {
    if (!CharArguments(swapper, inData, length, outData, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(inData);
    for (std::int32_t count = length; count > 0; --count) {
        std::uint8_t character = *source++;
        if (!EbcdicInvariant(character)) {
            udata_printError_nid_postfix(swapper,
                "uprv_copyEbcdic() string[%] contains a variant character in position %d\n", length,
                length - count);
            *status = Icu::InvalidCharacter;
            return 0;
        }
    }
    if (length > 0 && inData != outData) std::memcpy(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t FirstCharacter(std::uint8_t character, bool ebcdic) {
    if (!ebcdic) return IsInvariant(character) ? character : -1;
    if (character == 0) return 0;
    const std::int32_t mapped = AsciiFromEbcdicTable[character];
    return mapped != 0 && IsInvariant(static_cast<std::uint32_t>(mapped)) ? mapped : -1;
}

std::int32_t CompareInvariant(const char* outString, std::int32_t outLength, const char16_t* localString,
                              std::int32_t localLength, bool ebcdic) {
    if (!outString || outLength < -1 || !localString || localLength < -1) return 0;
    if (outLength < 0) outLength = static_cast<std::int32_t>(std::strlen(outString));
    if (localLength < 0) localLength = static_cast<std::int32_t>(std::char_traits<char16_t>::length(localString));
    std::int32_t remaining = outLength < localLength ? outLength : localLength;
    while (remaining > 0) {
        std::int32_t first = FirstCharacter(static_cast<std::uint8_t>(*outString++), ebcdic);
        std::int32_t second = *localString++;
        if (!IsInvariant(static_cast<std::uint32_t>(second))) second = -2;
        if ((first -= second) != 0) return first;
        --remaining;
    }
    return outLength - localLength;
}

std::int32_t APS5_VABI CompareInvAscii(const Icu::DataSwapper*, const char* outString, std::int32_t outLength,
                                       const char16_t* localString, std::int32_t localLength) {
    return CompareInvariant(outString, outLength, localString, localLength, false);
}

std::int32_t APS5_VABI CompareInvEbcdic(const Icu::DataSwapper*, const char* outString, std::int32_t outLength,
                                        const char16_t* localString, std::int32_t localLength) {
    return CompareInvariant(outString, outLength, localString, localLength, true);
}

bool LooksLikeData(const DataHeader* header) {
    return header->dataHeader.magic1 == MagicFirst && header->dataHeader.magic2 == MagicSecond &&
           header->info.sizeofUChar == 2;
}

}

extern "C" {

std::int16_t APS5_VABI udata_readInt16_nid_postfix(const Icu::DataSwapper* swapper, std::int16_t value) {
    if (!swapper) APS5_INVALID_ARG_EX;
    return static_cast<std::int16_t>(swapper->readUInt16(static_cast<std::uint16_t>(value)));
}

std::int32_t APS5_VABI udata_readInt32_nid_postfix(const Icu::DataSwapper* swapper, std::int32_t value) {
    if (!swapper) APS5_INVALID_ARG_EX;
    return static_cast<std::int32_t>(swapper->readUInt32(static_cast<std::uint32_t>(value)));
}

std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const Icu::DataSwapper* swapper, const void* inData,
                                                           std::int32_t length, void* outData, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (!swapper || !inData || length < 0 || (length > 0 && !outData)) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    const char* inChars = static_cast<const char*>(inData);
    std::int32_t stringsLength = length;
    while (stringsLength > 0 && inChars[stringsLength - 1] != 0) --stringsLength;
    swapper->swapInvChars(swapper, inData, stringsLength, outData, status);
    if (inData != outData && length > stringsLength) {
        std::memcpy(static_cast<char*>(outData) + stringsLength, inChars + stringsLength,
                    static_cast<std::size_t>(length - stringsLength));
    }
    return *status <= 0 ? length : 0;
}

void APS5_VABI udata_printError_nid_postfix(const Icu::DataSwapper* swapper, const char* format, ...) {
    if (!swapper) APS5_INVALID_ARG_EX;
    if (!swapper->printError) return;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    swapper->printError(swapper->printErrorContext, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const Icu::DataSwapper* swapper, const void* inData,
                                                       std::int32_t length, void* outData, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (!swapper || !inData || length < -1 || (length > 0 && !outData)) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    const auto* header = static_cast<const DataHeader*>(inData);
    if ((length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) || !LooksLikeData(header)) {
        udata_printError_nid_postfix(swapper, "udata_swapDataHeader(): initial bytes do not look like ICU data\n");
        *status = Icu::UnsupportedError;
        return 0;
    }
    const std::uint16_t headerSize = swapper->readUInt16(Load<std::uint16_t>(&header->dataHeader.headerSize));
    std::uint16_t infoSize = swapper->readUInt16(Load<std::uint16_t>(&header->info.size));
    if (headerSize < sizeof(DataHeader) || infoSize < sizeof(UDataInfo) ||
        headerSize < (sizeof(header->dataHeader) + infoSize) || (length >= 0 && length < headerSize)) {
        udata_printError_nid_postfix(swapper,
            "udata_swapDataHeader(): header size mismatch - headerSize %d infoSize %d length %d\n", headerSize,
            infoSize, length);
        *status = IndexOutOfBounds;
        return 0;
    }
    if (length > 0) {
        if (inData != outData) std::memcpy(outData, inData, headerSize);
        auto* outHeader = static_cast<DataHeader*>(outData);
        outHeader->info.isBigEndian = static_cast<std::uint8_t>(swapper->outIsBigEndian);
        outHeader->info.charsetFamily = swapper->outCharset;
        swapper->swapArray16(swapper, &header->dataHeader.headerSize, 2, &outHeader->dataHeader.headerSize, status);
        swapper->swapArray16(swapper, &header->info.size, 4, &outHeader->info.size, status);
        infoSize = static_cast<std::uint16_t>(infoSize + sizeof(header->dataHeader));
        const char* text = static_cast<const char*>(inData) + infoSize;
        const std::int32_t maxLength = headerSize - infoSize;
        for (length = 0; length < maxLength && text[length] != 0; ++length) {
        }
        swapper->swapInvChars(swapper, text, length, static_cast<char*>(outData) + infoSize, status);
    }
    return headerSize;
}

Icu::DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset,
                                                         std::int8_t outIsBigEndian, std::uint8_t outCharset,
                                                         std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (inCharset > EbcdicFamily || outCharset > EbcdicFamily) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    auto* swapper = static_cast<Icu::DataSwapper*>(uprv_malloc_nid_postfix(sizeof(Icu::DataSwapper)));
    if (!swapper) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    std::memset(swapper, 0, sizeof(Icu::DataSwapper));
    swapper->inIsBigEndian = inIsBigEndian;
    swapper->inCharset = inCharset;
    swapper->outIsBigEndian = outIsBigEndian;
    swapper->outCharset = outCharset;
    swapper->readUInt16 = inIsBigEndian == GuestIsBigEndian ? ReadDirectUInt16 : ReadSwapUInt16;
    swapper->readUInt32 = inIsBigEndian == GuestIsBigEndian ? ReadDirectUInt32 : ReadSwapUInt32;
    swapper->writeUInt16 = outIsBigEndian == GuestIsBigEndian ? WriteDirectUInt16 : WriteSwapUInt16;
    swapper->writeUInt32 = outIsBigEndian == GuestIsBigEndian ? WriteDirectUInt32 : WriteSwapUInt32;
    swapper->compareInvChars = outCharset == AsciiFamily ? CompareInvAscii : CompareInvEbcdic;
    if (inIsBigEndian == outIsBigEndian) {
        swapper->swapArray16 = CopyArray16;
        swapper->swapArray32 = CopyArray32;
        swapper->swapArray64 = CopyArray64;
    } else {
        swapper->swapArray16 = SwapArray16;
        swapper->swapArray32 = SwapArray32;
        swapper->swapArray64 = SwapArray64;
    }
    if (inCharset == AsciiFamily) {
        swapper->swapInvChars = outCharset == AsciiFamily ? CopyAscii : EbcdicFromAscii;
    } else {
        swapper->swapInvChars = outCharset == EbcdicFamily ? CopyEbcdic : AsciiFromEbcdic;
    }
    return swapper;
}

Icu::DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length,
                                                                     std::int8_t outIsBigEndian,
                                                                     std::uint8_t outCharset, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (!data || (length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) ||
        outCharset > EbcdicFamily) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    const auto* header = static_cast<const DataHeader*>(data);
    if (!LooksLikeData(header)) {
        *status = Icu::UnsupportedError;
        return nullptr;
    }
    const auto inIsBigEndian = static_cast<std::int8_t>(header->info.isBigEndian);
    const auto inCharset = static_cast<std::uint8_t>(header->info.charsetFamily);
    std::uint16_t headerSize = Load<std::uint16_t>(&header->dataHeader.headerSize);
    std::uint16_t infoSize = Load<std::uint16_t>(&header->info.size);
    if (inIsBigEndian != GuestIsBigEndian) {
        headerSize = SwapUInt16(headerSize);
        infoSize = SwapUInt16(infoSize);
    }
    if (headerSize < sizeof(DataHeader) || infoSize < sizeof(UDataInfo) ||
        headerSize < (sizeof(header->dataHeader) + infoSize) || (length >= 0 && length < headerSize)) {
        *status = Icu::UnsupportedError;
        return nullptr;
    }
    return udata_openSwapper_nid_postfix(inIsBigEndian, inCharset, outIsBigEndian, outCharset, status);
}

void APS5_VABI udata_closeSwapper_nid_postfix(Icu::DataSwapper* swapper) {
    IcuReleaseMemory_nid_no_patch(swapper);
}

}
