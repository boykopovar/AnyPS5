// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "DataSwapper.hpp"
#include <cstring>
#include <stdexcept>
#include <cstdio>

namespace {

using namespace Icu::Abi;

constexpr std::uint8_t AsciiFamily = 0;
constexpr std::uint8_t EbcdicFamily = 1;

constexpr std::uint32_t UtrieSig = 0x54726965;
constexpr std::uint32_t UtrieOeSig = 0x65697254;
constexpr std::uint32_t Utrie2Sig = 0x54726932;
constexpr std::uint32_t Utrie2OeSig = 0x32697254;
constexpr std::uint32_t UcptrieSig = 0x54726933;
constexpr std::uint32_t UcptrieOeSig = 0x33697254;

const std::uint8_t AsciiFromEbcdic[256] = {
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

const std::uint8_t EbcdicFromAscii[256] = {
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

const std::uint32_t InvariantChars[4] = {
    0xfffffbff,
    0xffffffe5,
    0x87fffffe,
    0x87fffffe
};

inline bool IsInvariant(std::uint8_t c) {
    return c <= 0x7f && (InvariantChars[c >> 5] & (static_cast<std::uint32_t>(1) << (c & 0x1f))) != 0;
}

std::int32_t APS5_VABI SwapArray16(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 1) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    const auto* p = static_cast<const std::uint16_t*>(inData);
    auto* q = static_cast<std::uint16_t*>(outData);
    auto count = length / 2;
    while (count-- > 0) {
        auto x = *p++;
        *q++ = static_cast<std::uint16_t>((x << 8) | (x >> 8));
    }
    return length;
}

std::int32_t APS5_VABI CopyArray16(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 1) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    if (length > 0 && inData != outData) std::memmove(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI SwapArray32(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 3) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    const auto* p = static_cast<const std::uint32_t*>(inData);
    auto* q = static_cast<std::uint32_t*>(outData);
    auto count = length / 4;
    while (count-- > 0) {
        auto x = *p++;
        *q++ = (x << 24) | ((x << 8) & 0xff0000u) | ((x >> 8) & 0xff00u) | (x >> 24);
    }
    return length;
}

std::int32_t APS5_VABI CopyArray32(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 3) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    if (length > 0 && inData != outData) std::memmove(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI SwapArray64(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 7) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    const auto* p = static_cast<const std::uint64_t*>(inData);
    auto* q = static_cast<std::uint64_t*>(outData);
    auto count = length / 8;
    while (count-- > 0) {
        auto x = *p++;
        x = (x << 56) | ((x & 0xff00ULL) << 40) | ((x & 0xff0000ULL) << 24) | ((x & 0xff000000ULL) << 8) |
            ((x >> 8) & 0xff000000ULL) | ((x >> 24) & 0xff0000ULL) | ((x >> 40) & 0xff00ULL) | (x >> 56);
        *q++ = x;
    }
    return length;
}

std::int32_t APS5_VABI CopyArray64(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length & 7) != 0 || !outData) {
        *errorCode = 1;
        return 0;
    }
    if (length > 0 && inData != outData) std::memmove(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::uint16_t APS5_VABI ReadSwapUInt16(std::uint16_t x) {
    return static_cast<std::uint16_t>((x << 8) | (x >> 8));
}

std::uint16_t APS5_VABI ReadDirectUInt16(std::uint16_t x) {
    return x;
}

std::uint32_t APS5_VABI ReadSwapUInt32(std::uint32_t x) {
    return (x << 24) | ((x << 8) & 0xff0000u) | ((x >> 8) & 0xff00u) | (x >> 24);
}

std::uint32_t APS5_VABI ReadDirectUInt32(std::uint32_t x) {
    return x;
}

void APS5_VABI WriteSwapUInt16(std::uint16_t* p, std::uint16_t x) {
    *p = static_cast<std::uint16_t>((x << 8) | (x >> 8));
}

void APS5_VABI WriteDirectUInt16(std::uint16_t* p, std::uint16_t x) {
    *p = x;
}

void APS5_VABI WriteSwapUInt32(std::uint32_t* p, std::uint32_t x) {
    *p = (x << 24) | ((x << 8) & 0xff0000u) | ((x >> 8) & 0xff00u) | (x >> 24);
}

void APS5_VABI WriteDirectUInt32(std::uint32_t* p, std::uint32_t x) {
    *p = x;
}

std::int32_t APS5_VABI CompareInvAscii(const DataSwapper*, const char* outString, std::int32_t outLength, const char16_t* localString, std::int32_t localLength) {
    if (!outString || outLength < -1 || !localString || localLength < -1) return 0;
    if (outLength < 0) outLength = static_cast<std::int32_t>(std::strlen(outString));
    if (localLength < 0) {
        localLength = 0;
        while (localString[localLength] != 0) ++localLength;
    }
    auto minLength = outLength < localLength ? outLength : localLength;
    while (minLength-- > 0) {
        auto c = static_cast<std::uint8_t>(*outString++);
        auto c1 = IsInvariant(c) ? static_cast<std::int32_t>(c) : -1;
        auto c2 = static_cast<std::int32_t>(*localString++);
        if (c2 > 0x7f || !IsInvariant(static_cast<std::uint8_t>(c2))) c2 = -2;
        if ((c1 -= c2) != 0) return c1;
    }
    return outLength - localLength;
}

std::int32_t APS5_VABI CompareInvEbcdic(const DataSwapper*, const char* outString, std::int32_t outLength, const char16_t* localString, std::int32_t localLength) {
    if (!outString || outLength < -1 || !localString || localLength < -1) return 0;
    if (outLength < 0) outLength = static_cast<std::int32_t>(std::strlen(outString));
    if (localLength < 0) {
        localLength = 0;
        while (localString[localLength] != 0) ++localLength;
    }
    auto minLength = outLength < localLength ? outLength : localLength;
    while (minLength-- > 0) {
        auto c = static_cast<std::uint8_t>(*outString++);
        std::int32_t c1;
        if (c == 0) c1 = 0;
        else if ((c1 = AsciiFromEbcdic[c]) != 0 && IsInvariant(static_cast<std::uint8_t>(c1))) {}
        else c1 = -1;
        auto c2 = static_cast<std::int32_t>(*localString++);
        if (c2 > 0x7f || !IsInvariant(static_cast<std::uint8_t>(c2))) c2 = -2;
        if ((c1 -= c2) != 0) return c1;
    }
    return outLength - localLength;
}

std::int32_t APS5_VABI CopyAscii(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* s = static_cast<const std::uint8_t*>(inData);
    for (std::int32_t i = 0; i < length; ++i) {
        if (!IsInvariant(s[i])) {
            udata_printError_nid_postfix(ds, "uprv_copyFromAscii() string[%d] contains a variant character in position %d\n", length, i);
            *errorCode = 10;
            return 0;
        }
    }
    if (length > 0 && inData != outData) std::memmove(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI CopyEbcdic(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* s = static_cast<const std::uint8_t*>(inData);
    for (std::int32_t i = 0; i < length; ++i) {
        auto c = s[i];
        if (c != 0 && ((c = AsciiFromEbcdic[c]) == 0 || !IsInvariant(c))) {
            udata_printError_nid_postfix(ds, "uprv_copyFromEbcdic() string[%d] contains a variant character in position %d\n", length, i);
            *errorCode = 10;
            return 0;
        }
    }
    if (length > 0 && inData != outData) std::memmove(outData, inData, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI AsciiFromEbcdicSwap(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* s = static_cast<const std::uint8_t*>(inData);
    auto* t = static_cast<std::uint8_t*>(outData);
    for (std::int32_t i = 0; i < length; ++i) {
        auto c = s[i];
        if (c != 0 && ((c = AsciiFromEbcdic[c]) == 0 || !IsInvariant(c))) {
            udata_printError_nid_postfix(ds, "uprv_asciiFromEbcdic() string[%d] contains a variant character in position %d\n", length, i);
            *errorCode = 10;
            return 0;
        }
        t[i] = c;
    }
    return length;
}

std::int32_t APS5_VABI EbcdicFromAsciiSwap(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* s = static_cast<const std::uint8_t*>(inData);
    auto* t = static_cast<std::uint8_t*>(outData);
    for (std::int32_t i = 0; i < length; ++i) {
        auto c = s[i];
        if (!IsInvariant(c)) {
            udata_printError_nid_postfix(ds, "uprv_ebcdicFromAscii() string[%d] contains a variant character in position %d\n", length, i);
            *errorCode = 10;
            return 0;
        }
        t[i] = EbcdicFromAscii[c];
    }
    return length;
}

struct MappedData {
    std::uint16_t headerSize;
    std::uint8_t magic1;
    std::uint8_t magic2;
};

struct DataInfo {
    std::uint16_t size;
    std::uint16_t reservedWord;
    std::uint8_t isBigEndian;
    std::uint8_t charsetFamily;
    std::uint8_t sizeofUChar;
    std::uint8_t reservedByte;
    std::uint8_t dataFormat[4];
    std::uint8_t formatVersion[4];
    std::uint8_t dataVersion[4];
};

struct DataHeader {
    MappedData dataHeader;
    DataInfo info;
};

struct UTrieHeader {
    std::uint32_t signature;
    std::uint32_t options;
    std::int32_t indexLength;
    std::int32_t dataLength;
};

struct UTrie2Header {
    std::uint32_t signature;
    std::uint16_t options;
    std::uint16_t indexLength;
    std::uint16_t shiftedDataLength;
    std::uint16_t index2NullOffset;
    std::uint16_t dataNullOffset;
    std::uint16_t shiftedHighStart;
};

struct UCPTrieHeader {
    std::uint32_t signature;
    std::uint16_t options;
    std::uint16_t indexLength;
    std::uint16_t dataLength;
    std::uint16_t index3NullOffset;
    std::uint16_t dataNullOffset;
    std::uint16_t shiftedHighStart;
};

std::int32_t SwapUTrie(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || (length >= 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    if (length >= 0 && static_cast<std::size_t>(length) < sizeof(UTrieHeader)) {
        *errorCode = 8;
        return 0;
    }
    const auto* inTrie = static_cast<const UTrieHeader*>(inData);
    UTrieHeader trie{};
    trie.signature = ds->readUInt32(inTrie->signature);
    trie.options = ds->readUInt32(inTrie->options);
    trie.indexLength = udata_readInt32_nid_postfix(ds, inTrie->indexLength);
    trie.dataLength = udata_readInt32_nid_postfix(ds, inTrie->dataLength);

    if (trie.signature != UtrieSig ||
        (trie.options & 0xf) != 5 ||
        ((trie.options >> 4) & 0xf) != 2 ||
        trie.indexLength < (0x10000 >> 5) ||
        (trie.indexLength & ((1 << 5) - 1)) != 0 ||
        trie.dataLength < (1 << 5) ||
        (trie.dataLength & 3) != 0 ||
        ((trie.options & 0x200) != 0 && trie.dataLength < ((1 << 5) + 0x100))) {
        *errorCode = 3;
        return 0;
    }
    bool dataIs32 = (trie.options & 0x100) != 0;
    auto size = static_cast<std::int32_t>(sizeof(UTrieHeader)) + trie.indexLength * 2 + trie.dataLength * (dataIs32 ? 4 : 2);
    if (length >= 0) {
        if (length < size) {
            *errorCode = 8;
            return 0;
        }
        auto* outTrie = static_cast<UTrieHeader*>(outData);
        ds->swapArray32(ds, inTrie, sizeof(UTrieHeader), outTrie, errorCode);
        if (dataIs32) {
            ds->swapArray16(ds, inTrie + 1, trie.indexLength * 2, outTrie + 1, errorCode);
            ds->swapArray32(ds, reinterpret_cast<const std::uint16_t*>(inTrie + 1) + trie.indexLength, trie.dataLength * 4,
                            reinterpret_cast<std::uint16_t*>(outTrie + 1) + trie.indexLength, errorCode);
        } else {
            ds->swapArray16(ds, inTrie + 1, (trie.indexLength + trie.dataLength) * 2, outTrie + 1, errorCode);
        }
    }
    return size;
}

std::int32_t GetTrieVersion(const void* data, std::int32_t length) {
    if (length < 16 || !data || (reinterpret_cast<std::uintptr_t>(data) & 3) != 0) return 0;
    auto signature = *static_cast<const std::uint32_t*>(data);
    if (signature == UcptrieSig || signature == UcptrieOeSig) return 3;
    if (signature == Utrie2Sig || signature == Utrie2OeSig) return 2;
    if (signature == UtrieSig || signature == UtrieOeSig) return 1;
    return 0;
}

}

extern "C" {

DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return nullptr;
    if (inCharset > EbcdicFamily || outCharset > EbcdicFamily) {
        *errorCode = 1;
        return nullptr;
    }
    auto* swapper = static_cast<DataSwapper*>(Allocate_nid_no_patch(sizeof(DataSwapper)));
    if (!swapper) {
        *errorCode = 7;
        return nullptr;
    }
    std::memset(swapper, 0, sizeof(DataSwapper));
    swapper->inIsBigEndian = inIsBigEndian;
    swapper->inCharset = inCharset;
    swapper->outIsBigEndian = outIsBigEndian;
    swapper->outCharset = outCharset;

    swapper->readUInt16 = inIsBigEndian ? ReadSwapUInt16 : ReadDirectUInt16;
    swapper->readUInt32 = inIsBigEndian ? ReadSwapUInt32 : ReadDirectUInt32;
    swapper->writeUInt16 = outIsBigEndian ? WriteSwapUInt16 : WriteDirectUInt16;
    swapper->writeUInt32 = outIsBigEndian ? WriteSwapUInt32 : WriteDirectUInt32;

    swapper->compareInvChars = (outCharset == AsciiFamily) ? CompareInvAscii : CompareInvEbcdic;

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
        swapper->swapInvChars = (outCharset == AsciiFamily) ? CopyAscii : EbcdicFromAsciiSwap;
    } else {
        swapper->swapInvChars = (outCharset == EbcdicFamily) ? CopyEbcdic : AsciiFromEbcdicSwap;
    }
    return swapper;
}

DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return nullptr;
    if (!data || (length >= 0 && static_cast<std::size_t>(length) < sizeof(DataHeader)) || outCharset > EbcdicFamily) {
        *errorCode = 1;
        return nullptr;
    }
    const auto* pHeader = static_cast<const DataHeader*>(data);
    if ((length >= 0 && static_cast<std::size_t>(length) < sizeof(DataHeader)) ||
        pHeader->dataHeader.magic1 != 0xda ||
        pHeader->dataHeader.magic2 != 0x27 ||
        pHeader->info.sizeofUChar != 2) {
        *errorCode = 16;
        return nullptr;
    }
    auto inIsBigEndian = static_cast<std::int8_t>(pHeader->info.isBigEndian);
    auto inCharset = pHeader->info.charsetFamily;

    std::uint16_t headerSize, infoSize;
    if (!inIsBigEndian) {
        headerSize = pHeader->dataHeader.headerSize;
        infoSize = pHeader->info.size;
    } else {
        headerSize = ReadSwapUInt16(pHeader->dataHeader.headerSize);
        infoSize = ReadSwapUInt16(pHeader->info.size);
    }
    if (headerSize < sizeof(DataHeader) ||
        infoSize < sizeof(DataInfo) ||
        headerSize < (sizeof(MappedData) + infoSize) ||
        (length >= 0 && length < headerSize)) {
        *errorCode = 16;
        return nullptr;
    }
    return udata_openSwapper_nid_postfix(inIsBigEndian, inCharset, outIsBigEndian, outCharset, errorCode);
}

void APS5_VABI udata_closeSwapper_nid_postfix(DataSwapper* swapper) {
    Free_nid_no_patch(swapper);
}

std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < -1 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* pHeader = static_cast<const DataHeader*>(inData);
    if ((length >= 0 && static_cast<std::size_t>(length) < sizeof(DataHeader)) ||
        pHeader->dataHeader.magic1 != 0xda ||
        pHeader->dataHeader.magic2 != 0x27 ||
        pHeader->info.sizeofUChar != 2) {
        udata_printError_nid_postfix(ds, "udata_swapDataHeader(): initial bytes do not look like ICU data\n");
        *errorCode = 16;
        return 0;
    }
    auto headerSize = ds->readUInt16(pHeader->dataHeader.headerSize);
    auto infoSize = ds->readUInt16(pHeader->info.size);
    if (headerSize < sizeof(DataHeader) ||
        infoSize < sizeof(DataInfo) ||
        headerSize < (sizeof(MappedData) + infoSize) ||
        (length >= 0 && length < headerSize)) {
        udata_printError_nid_postfix(ds, "udata_swapDataHeader(): header size mismatch - headerSize %d infoSize %d length %d\n", headerSize, infoSize, length);
        *errorCode = 8;
        return 0;
    }
    if (length > 0) {
        if (inData != outData) std::memmove(outData, inData, headerSize);
        auto* outHeader = static_cast<DataHeader*>(outData);
        outHeader->info.isBigEndian = ds->outIsBigEndian;
        outHeader->info.charsetFamily = ds->outCharset;
        ds->swapArray16(ds, &pHeader->dataHeader.headerSize, 2, &outHeader->dataHeader.headerSize, errorCode);
        ds->swapArray16(ds, &pHeader->info.size, 4, &outHeader->info.size, errorCode);
        infoSize += sizeof(MappedData);
        const auto* s = static_cast<const char*>(inData) + infoSize;
        auto maxLength = headerSize - infoSize;
        std::int32_t strLen = 0;
        for (; strLen < maxLength && s[strLen] != 0; ++strLen) {}
        ds->swapInvChars(ds, s, strLen, static_cast<char*>(outData) + infoSize, errorCode);
    }
    return headerSize;
}

std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode || *errorCode > 0) return 0;
    if (!ds || !inData || length < 0 || (length > 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    const auto* inChars = static_cast<const char*>(inData);
    auto stringsLength = length;
    while (stringsLength > 0 && inChars[stringsLength - 1] != 0) --stringsLength;
    ds->swapInvChars(ds, inData, stringsLength, outData, errorCode);
    if (inData != outData && length > stringsLength) {
        std::memmove(static_cast<char*>(outData) + stringsLength, inChars + stringsLength, static_cast<std::size_t>(length - stringsLength));
    }
    return *errorCode <= 0 ? length : 0;
}

std::int16_t APS5_VABI udata_readInt16_nid_postfix(const DataSwapper* ds, std::int16_t x) {
    return static_cast<std::int16_t>(ds->readUInt16(static_cast<std::uint16_t>(x)));
}

std::int32_t APS5_VABI udata_readInt32_nid_postfix(const DataSwapper* ds, std::int32_t x) {
    return static_cast<std::int32_t>(ds->readUInt32(static_cast<std::uint32_t>(x)));
}

void APS5_VABI udata_printError_nid_postfix(const DataSwapper* ds, const char* fmt, ...) {
    if (!ds) throw std::invalid_argument("null UDataSwapper");
    if (!ds->printError) return;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    ds->printError(ds->printErrorContext, fmt, reinterpret_cast<void*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    ds->printError(ds->printErrorContext, fmt, reinterpret_cast<void*>(args));
    va_end(args);
#endif
}

std::int32_t APS5_VABI utrie2_swap_nid_postfix(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode) throw std::invalid_argument("null UErrorCode");
    if (*errorCode > 0) return 0;
    if (!ds || !inData || (length >= 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    if (length >= 0 && static_cast<std::size_t>(length) < sizeof(UTrie2Header)) {
        *errorCode = 8;
        return 0;
    }
    const auto* inTrie = static_cast<const UTrie2Header*>(inData);
    UTrie2Header trie{};
    trie.signature = ds->readUInt32(inTrie->signature);
    trie.options = ds->readUInt16(inTrie->options);
    trie.indexLength = ds->readUInt16(inTrie->indexLength);
    trie.shiftedDataLength = ds->readUInt16(inTrie->shiftedDataLength);

    auto valueBits = trie.options & 0xf;
    auto dataLength = static_cast<std::int32_t>(trie.shiftedDataLength) << 2;
    if (trie.signature != Utrie2Sig || valueBits > 1 || trie.indexLength < 0x840 || dataLength < 0xc0) {
        *errorCode = 3;
        return 0;
    }
    auto size = static_cast<std::int32_t>(sizeof(UTrie2Header)) + trie.indexLength * 2;
    if (valueBits == 0) size += dataLength * 2;
    else size += dataLength * 4;

    if (length >= 0) {
        if (length < size) {
            *errorCode = 8;
            return 0;
        }
        auto* outTrie = static_cast<UTrie2Header*>(outData);
        ds->swapArray32(ds, &inTrie->signature, 4, &outTrie->signature, errorCode);
        ds->swapArray16(ds, &inTrie->options, sizeof(UTrie2Header) - 4, &outTrie->options, errorCode);
        const auto* in16 = reinterpret_cast<const std::uint16_t*>(inTrie + 1);
        auto* out16 = reinterpret_cast<std::uint16_t*>(outTrie + 1);
        ds->swapArray16(ds, in16, trie.indexLength * 2, out16, errorCode);
        in16 += trie.indexLength;
        out16 += trie.indexLength;
        if (valueBits == 0) {
            ds->swapArray16(ds, in16, dataLength * 2, out16, errorCode);
        } else {
            ds->swapArray32(ds, in16, dataLength * 4, out16, errorCode);
        }
    }
    return size;
}

std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode) throw std::invalid_argument("null UErrorCode");
    if (*errorCode > 0) return 0;
    if (!ds || !inData || (length >= 0 && !outData)) {
        *errorCode = 1;
        return 0;
    }
    if (length >= 0 && static_cast<std::size_t>(length) < sizeof(UCPTrieHeader)) {
        *errorCode = 8;
        return 0;
    }
    const auto* inTrie = static_cast<const UCPTrieHeader*>(inData);
    UCPTrieHeader trie{};
    trie.signature = ds->readUInt32(inTrie->signature);
    trie.options = ds->readUInt16(inTrie->options);
    trie.indexLength = ds->readUInt16(inTrie->indexLength);
    trie.dataLength = ds->readUInt16(inTrie->dataLength);

    auto type = (trie.options >> 6) & 3;
    auto valueWidth = trie.options & 7;
    auto dataLength = (static_cast<std::int32_t>(trie.options & 0xf000) << 4) | trie.dataLength;
    std::int32_t minIndexLength = (type == 0) ? 0x400 : 0x40;

    if (trie.signature != UcptrieSig || type > 1 || (trie.options & 0x38) != 0 || valueWidth > 2 || trie.indexLength < minIndexLength || dataLength < 0x80) {
        *errorCode = 3;
        return 0;
    }
    auto size = static_cast<std::int32_t>(sizeof(UCPTrieHeader)) + trie.indexLength * 2;
    if (valueWidth == 0) size += dataLength * 2;
    else if (valueWidth == 1) size += dataLength * 4;
    else size += dataLength;

    if (length >= 0) {
        if (length < size) {
            *errorCode = 8;
            return 0;
        }
        auto* outTrie = static_cast<UCPTrieHeader*>(outData);
        ds->swapArray32(ds, &inTrie->signature, 4, &outTrie->signature, errorCode);
        ds->swapArray16(ds, &inTrie->options, 12, &outTrie->options, errorCode);
        const auto* inIndex = reinterpret_cast<const std::uint16_t*>(inTrie + 1);
        auto* outIndex = reinterpret_cast<std::uint16_t*>(outTrie + 1);
        ds->swapArray16(ds, inIndex, trie.indexLength * 2, outIndex, errorCode);
        const auto* inDataPtr = inIndex + trie.indexLength;
        auto* outDataPtr = outIndex + trie.indexLength;
        if (valueWidth == 0) {
            ds->swapArray16(ds, inDataPtr, dataLength * 2, outDataPtr, errorCode);
        } else if (valueWidth == 1) {
            ds->swapArray32(ds, inDataPtr, dataLength * 4, outDataPtr, errorCode);
        } else {
            if (inTrie != outTrie) std::memmove(outDataPtr, inDataPtr, static_cast<std::size_t>(dataLength));
        }
    }
    return size;
}

std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode) {
    if (!errorCode) throw std::invalid_argument("null UErrorCode");
    if (*errorCode > 0) return 0;
    switch (GetTrieVersion(inData, length)) {
    case 1:
        return SwapUTrie(ds, inData, length, outData, errorCode);
    case 2:
        return utrie2_swap_nid_postfix(ds, inData, length, outData, errorCode);
    case 3:
        return ucptrie_swap_nid_postfix(ds, inData, length, outData, errorCode);
    default:
        *errorCode = 3;
        return 0;
    }
}

}
