// Derived from icu4c/source/common/udataswp.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2)
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2003-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*/

#include "prx/libSceIcu/DataSwap.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "udatamem.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

static_assert(sizeof(UDataSwapper) == 96);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::uint8_t AsciiFamily = 0;
constexpr std::uint8_t EbcdicFamily = 1;
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr bool HostIsBigEndian = true;
#else
constexpr bool HostIsBigEndian = false;
#endif

bool Failed(const std::int32_t* status) {
    return status == nullptr || *status > 0;
}

template <typename Type>
std::int32_t APS5_VABI CopyArray(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output,
    std::int32_t* status) {
    (void)swapper;
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || output == nullptr ||
        static_cast<std::uint32_t>(length) % sizeof(Type) != 0) {
        *status = IllegalArgument;
        return 0;
    }
    if (length > 0 && input != output) std::memcpy(output, input, static_cast<std::size_t>(length));
    return length;
}

std::uint16_t APS5_VABI ReadDirectUInt16(std::uint16_t value) {
    return value;
}

std::uint16_t APS5_VABI ReadSwapUInt16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t APS5_VABI ReadDirectUInt32(std::uint32_t value) {
    return value;
}

std::uint32_t APS5_VABI ReadSwapUInt32(std::uint32_t value) {
    return (value << 24) | ((value << 8) & 0xff0000u) | ((value >> 8) & 0xff00u) | (value >> 24);
}

void APS5_VABI WriteDirectUInt16(std::uint16_t* destination, std::uint16_t value) {
    *destination = value;
}

void APS5_VABI WriteSwapUInt16(std::uint16_t* destination, std::uint16_t value) {
    *destination = ReadSwapUInt16(value);
}

void APS5_VABI WriteDirectUInt32(std::uint32_t* destination, std::uint32_t value) {
    *destination = value;
}

void APS5_VABI WriteSwapUInt32(std::uint32_t* destination, std::uint32_t value) {
    *destination = ReadSwapUInt32(value);
}

std::int32_t APS5_VABI SwapArray16(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output,
    std::int32_t* status) {
    (void)swapper;
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || (length & 1) != 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* source = static_cast<const std::uint16_t*>(input);
    auto* destination = static_cast<std::uint16_t*>(output);
    for (std::int32_t count = length / 2; count > 0; --count) *destination++ = ReadSwapUInt16(*source++);
    return length;
}

std::int32_t APS5_VABI SwapArray32(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output,
    std::int32_t* status) {
    (void)swapper;
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || (length & 3) != 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* source = static_cast<const std::uint32_t*>(input);
    auto* destination = static_cast<std::uint32_t*>(output);
    for (std::int32_t count = length / 4; count > 0; --count) *destination++ = ReadSwapUInt32(*source++);
    return length;
}


std::uint64_t APS5_VABI ReadSwapUInt64(std::uint64_t value) {
    return (value << 56) | ((value & 0xff00ull) << 40) | ((value & 0xff0000ull) << 24) | ((value & 0xff000000ull) << 8) |
        ((value >> 8) & 0xff000000ull) | ((value >> 24) & 0xff0000ull) | ((value >> 40) & 0xff00ull) | (value >> 56);
}

std::int32_t APS5_VABI SwapArray64(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output,
    std::int32_t* status) {
    (void)swapper;
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || (length & 7) != 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* source = static_cast<const std::uint64_t*>(input);
    auto* destination = static_cast<std::uint64_t*>(output);
    for (std::int32_t count = length / 8; count > 0; --count) *destination++ = ReadSwapUInt64(*source++);
    return length;
}

std::int32_t APS5_VABI CopyAscii(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output,
    std::int32_t* status) {
    (void)swapper;
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    if (length > 0 && input != output) std::memcpy(output, input, static_cast<std::size_t>(length));
    return length;
}

std::int8_t APS5_VABI CompareInvAscii(const UDataSwapper* swapper, const char* output, std::int32_t outputLength,
    const char16_t* local, std::int32_t localLength) {
    (void)swapper;
    std::int32_t index = 0;
    for (; index < outputLength && index < localLength; ++index) {
        const std::int32_t difference = static_cast<std::uint8_t>(output[index]) - static_cast<std::int32_t>(local[index]);
        if (difference != 0) return static_cast<std::int8_t>(difference < 0 ? -1 : 1);
    }
    const std::int32_t difference = outputLength - localLength;
    return static_cast<std::int8_t>(difference < 0 ? -1 : difference > 0 ? 1 : 0);
}

}

extern "C" {

UDataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset,
    std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (inCharset > EbcdicFamily || outCharset > EbcdicFamily) {
        *status = IllegalArgument;
        return nullptr;
    }
    if (inCharset != AsciiFamily || outCharset != AsciiFamily) {
        *status = UnsupportedError;
        return nullptr;
    }
    auto* swapper = static_cast<UDataSwapper*>(uprv_malloc_nid_postfix(sizeof(UDataSwapper)));
    if (swapper == nullptr) {
        *status = MemoryAllocation;
        return nullptr;
    }
    std::memset(swapper, 0, sizeof(UDataSwapper));
    swapper->inIsBigEndian = inIsBigEndian;
    swapper->inCharset = inCharset;
    swapper->outIsBigEndian = outIsBigEndian;
    swapper->outCharset = outCharset;
    swapper->readUInt16 = inIsBigEndian == HostIsBigEndian ? ReadDirectUInt16 : ReadSwapUInt16;
    swapper->readUInt32 = inIsBigEndian == HostIsBigEndian ? ReadDirectUInt32 : ReadSwapUInt32;
    swapper->writeUInt16 = outIsBigEndian == HostIsBigEndian ? WriteDirectUInt16 : WriteSwapUInt16;
    swapper->writeUInt32 = outIsBigEndian == HostIsBigEndian ? WriteDirectUInt32 : WriteSwapUInt32;
    swapper->compareInvChars = CompareInvAscii;
    if (inIsBigEndian == outIsBigEndian) {
        swapper->swapArray16 = CopyArray<std::uint16_t>;
        swapper->swapArray32 = CopyArray<std::uint32_t>;
        swapper->swapArray64 = CopyArray<std::uint64_t>;
    } else {
        swapper->swapArray16 = SwapArray16;
        swapper->swapArray32 = SwapArray32;
        swapper->swapArray64 = SwapArray64;
    }
    swapper->swapInvChars = CopyAscii;
    return swapper;
}

void APS5_VABI udata_closeSwapper_nid_postfix(UDataSwapper* swapper) {
    if (swapper != nullptr) IcuReleaseMemory_nid_no_patch(swapper);
}

std::int16_t APS5_VABI udata_readInt16_nid_postfix(const UDataSwapper* swapper, std::int16_t value) {
    return static_cast<std::int16_t>(swapper->readUInt16(static_cast<std::uint16_t>(value)));
}

std::int32_t APS5_VABI udata_readInt32_nid_postfix(const UDataSwapper* swapper, std::int32_t value) {
    return static_cast<std::int32_t>(swapper->readUInt32(static_cast<std::uint32_t>(value)));
}

void APS5_VABI udata_printError_nid_postfix(const UDataSwapper* swapper, const char* format, ...) {
    if (swapper == nullptr || swapper->printError == nullptr) return;
    va_list arguments;
    va_start(arguments, format);
    swapper->printError(swapper->printErrorContext, format, arguments);
    va_end(arguments);
}

}

extern "C" std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const UDataSwapper* swapper,
    const void* input, std::int32_t length, void* output, std::int32_t* status) {
    if (swapper == nullptr) {
        if (status != nullptr && *status <= 0) *status = IllegalArgument;
        return 0;
    }
    if (Failed(status)) return 0;
    if (input == nullptr || length < 0 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* chars = static_cast<const char*>(input);
    std::int32_t stringsLength = length;
    while (stringsLength > 0 && chars[stringsLength - 1] != 0) --stringsLength;
    if (stringsLength > 0) swapper->swapInvChars(swapper, input, stringsLength, output, status);
    if (input != output && length > stringsLength && Failed(status) == false) {
        std::memcpy(static_cast<char*>(output) + stringsLength, chars + stringsLength,
            static_cast<std::size_t>(length - stringsLength));
    }
    return Failed(status) ? 0 : length;
}

extern "C" UDataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length,
    std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (data == nullptr || (length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) ||
        outCharset > EbcdicFamily) {
        *status = IllegalArgument;
        return nullptr;
    }
    const auto* header = static_cast<const DataHeader*>(data);
    if (header->dataHeader.magic1 != 0xda || header->dataHeader.magic2 != 0x27 || header->info.sizeofUChar != 2) {
        *status = UnsupportedError;
        return nullptr;
    }
    const std::int8_t inIsBigEndian = static_cast<std::int8_t>(header->info.isBigEndian);
    const std::uint8_t inCharset = header->info.charsetFamily;
    std::uint16_t headerSize = header->dataHeader.headerSize;
    std::uint16_t infoSize = header->info.size;
    if (inIsBigEndian != HostIsBigEndian) {
        headerSize = ReadSwapUInt16(headerSize);
        infoSize = ReadSwapUInt16(infoSize);
    }
    if (headerSize < sizeof(DataHeader) || infoSize < sizeof(UDataInfo) ||
        headerSize < sizeof(header->dataHeader) + infoSize ||
        (length >= 0 && length < static_cast<std::int32_t>(headerSize))) {
        *status = UnsupportedError;
        return nullptr;
    }
    return udata_openSwapper_nid_postfix(inIsBigEndian, inCharset, outIsBigEndian, outCharset, status);
}

extern "C" std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const UDataSwapper* swapper, const void* input,
    std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (swapper == nullptr || input == nullptr || length < -1 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* header = static_cast<const DataHeader*>(input);
    if ((length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) ||
        header->dataHeader.magic1 != 0xda || header->dataHeader.magic2 != 0x27 || header->info.sizeofUChar != 2) {
        udata_printError_nid_postfix(swapper, "udata_swapDataHeader(): initial bytes do not look like ICU data\n");
        *status = UnsupportedError;
        return 0;
    }
    const std::uint16_t headerSize = swapper->readUInt16(header->dataHeader.headerSize);
    const std::uint16_t infoSize = swapper->readUInt16(header->info.size);
    if (headerSize < sizeof(DataHeader) || infoSize < sizeof(UDataInfo) ||
        headerSize < sizeof(header->dataHeader) + infoSize ||
        (length >= 0 && length < static_cast<std::int32_t>(headerSize))) {
        udata_printError_nid_postfix(swapper,
            "udata_swapDataHeader(): header size mismatch - headerSize %d infoSize %d length %d\n",
            static_cast<int>(headerSize), static_cast<int>(infoSize), static_cast<int>(length));
        *status = 8;
        return 0;
    }
    if (length > 0) {
        if (input != output) std::memcpy(output, input, static_cast<std::size_t>(headerSize));
        auto* outHeader = static_cast<DataHeader*>(output);
        outHeader->info.isBigEndian = static_cast<std::uint8_t>(swapper->outIsBigEndian);
        outHeader->info.charsetFamily = swapper->outCharset;
        swapper->swapArray16(swapper, &header->dataHeader.headerSize, 2, &outHeader->dataHeader.headerSize, status);
        swapper->swapArray16(swapper, &header->info.size, 4, &outHeader->info.size, status);
        const std::int32_t copyrightStart = static_cast<std::int32_t>(sizeof(header->dataHeader)) + infoSize;
        const char* string = static_cast<const char*>(input) + copyrightStart;
        const std::int32_t stringLimit = static_cast<std::int32_t>(headerSize) - copyrightStart;
        std::int32_t stringLength = 0;
        while (stringLength < stringLimit && string[stringLength] != 0) ++stringLength;
        if (stringLength > 0) {
            swapper->swapInvChars(swapper, string, stringLength,
                static_cast<char*>(output) + copyrightStart, status);
        }
    }
    return headerSize;
}
