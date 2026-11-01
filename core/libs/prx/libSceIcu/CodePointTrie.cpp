// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using Trie = Icu::CodePointTrie;

struct BinaryHeader {
    std::uint32_t signature;
    std::uint16_t options;
    std::uint16_t indexLength;
    std::uint16_t dataLength;
    std::uint16_t index3NullOffset;
    std::uint16_t dataNullOffset;
    std::uint16_t shiftedHighStart;
};

static_assert(sizeof(BinaryHeader) == 16 && alignof(BinaryHeader) == 4);
static_assert(offsetof(BinaryHeader, options) == 4 && offsetof(BinaryHeader, shiftedHighStart) == 14);

void* Allocate(std::size_t size) {
    return std::malloc(size);
}

void Release(void* pointer) {
    std::free(pointer);
}

const Trie& Checked(const Trie* trie) {
    if (!trie) throw std::invalid_argument("UCPTrie: null trie");
    return *trie;
}

std::uint16_t Index(const Trie& trie, std::int32_t position) {
    if (!trie.index || position < 0 || position >= trie.indexLength)
        throw std::invalid_argument("UCPTrie: invalid index access");
    return trie.index[position];
}

std::uint32_t Value(const Trie& trie, std::int32_t position) {
    if (trie.valueWidth < 0 || trie.valueWidth > 2) return 0xffffffff;
    if (!trie.data.pointer || position < 0 || position >= trie.dataLength)
        throw std::invalid_argument("UCPTrie: invalid data access");
    if (trie.valueWidth == 0) return trie.data.words[position];
    if (trie.valueWidth == 1) {
        if ((reinterpret_cast<std::uintptr_t>(trie.data.pointer) & 3) != 0)
            throw std::invalid_argument("UCPTrie: unaligned data");
        return trie.data.dwords[position];
    }
    return trie.data.bytes[position];
}

std::int32_t DataBlock(const Trie& trie, std::int32_t block, std::int32_t offset) {
    if (!(block & 0x8000)) return Index(trie, block + offset);
    const auto group = (block & 0x7fff) + (offset & ~7) + (offset >> 3);
    const auto within = offset & 7;
    return ((static_cast<std::uint32_t>(Index(trie, group)) << (2 + 2 * within)) & 0x30000) |
        Index(trie, group + 1 + within);
}

std::int32_t SmallIndex(const Trie& trie, std::int32_t character) {
    const auto first = (character >> 14) + (trie.type == 0 ? 1020 : 64);
    const auto third = Index(trie, Index(trie, first) + ((character >> 9) & 31));
    return DataBlock(trie, third, (character >> 4) & 31) + (character & 15);
}

std::int32_t CodePointIndex(const Trie& trie, std::int32_t character, std::uint32_t fastMaximum) {
    if (static_cast<std::uint32_t>(character) <= fastMaximum)
        return Index(trie, character >> 6) + (character & 63);
    if (static_cast<std::uint32_t>(character) > 0x10ffff) return trie.dataLength - 1;
    return character >= trie.highStart ? trie.dataLength - 2 : SmallIndex(trie, character);
}

std::uint32_t Filter(std::uint32_t value, const Trie& trie, std::uint32_t filteredNull,
                     Icu::CodePointFilter filter, const void* context) {
    return value == trie.nullValue ? filteredNull : filter ? filter(context, value) : value;
}

std::int32_t Range(const Trie* pointer, std::int32_t start, Icu::CodePointFilter filter,
                   const void* context, std::uint32_t* output) {
    if (static_cast<std::uint32_t>(start) > 0x10ffff) return -1;
    const auto& trie = Checked(pointer);
    if (start >= trie.highStart) {
        if (output) {
            const auto value = Value(trie, trie.dataLength - 2);
            *output = filter ? filter(context, value) : value;
        }
        return 0x10ffff;
    }
    const auto nullValue = filter ? filter(context, trie.nullValue) : trie.nullValue;
    std::int32_t previousThird = -1;
    std::int32_t previousBlock = -1;
    auto character = start;
    std::uint32_t rawValue = 0;
    auto value = nullValue;
    bool haveValue = false;
    const auto acceptNull = [&] {
        if (haveValue) return value == nullValue;
        rawValue = trie.nullValue;
        value = nullValue;
        if (output) *output = value;
        haveValue = true;
        return true;
    };
    const auto acceptValue = [&](std::uint32_t raw) {
        if (!haveValue) {
            rawValue = raw;
            value = Filter(raw, trie, nullValue, filter, context);
            if (output) *output = value;
            haveValue = true;
        } else if (raw != rawValue) {
            if (!filter || Filter(raw, trie, nullValue, filter, context) != value) return false;
            rawValue = raw;
        }
        return true;
    };
    do {
        std::int32_t third = 0;
        std::int32_t offset;
        std::int32_t count;
        std::int32_t blockLength;
        if (character <= 0xffff && (trie.type == 0 || character <= 0xfff)) {
            offset = character >> 6;
            count = trie.type == 0 ? 1024 : 64;
            blockLength = 64;
        } else {
            const auto first = (character >> 14) + (trie.type == 0 ? 1020 : 64);
            third = Index(trie, Index(trie, first) + ((character >> 9) & 31));
            if (third == previousThird && character - start >= 512) {
                character += 512;
                continue;
            }
            previousThird = third;
            if (third == trie.index3NullOffset) {
                if (!acceptNull()) return character - 1;
                previousBlock = trie.dataNullOffset;
                character = (character + 512) & ~511;
                continue;
            }
            offset = (character >> 4) & 31;
            count = 32;
            blockLength = 16;
        }
        do {
            const auto block = DataBlock(trie, third, offset);
            if (block == previousBlock && character - start >= blockLength) {
                character += blockLength;
            } else {
                previousBlock = block;
                const auto mask = blockLength - 1;
                if (block == trie.dataNullOffset) {
                    if (!acceptNull()) return character - 1;
                    character = (character + blockLength) & ~mask;
                } else {
                    auto dataPosition = block + (character & mask);
                    if (!acceptValue(Value(trie, dataPosition))) return character - 1;
                    while ((++character & mask) != 0) {
                        if (!acceptValue(Value(trie, ++dataPosition))) return character - 1;
                    }
                }
            }
        } while (++offset < count);
    } while (character < trie.highStart);
    return Filter(Value(trie, trie.dataLength - 2), trie, nullValue, filter, context) == value ?
        0x10ffff : character - 1;
}

std::int32_t Width(std::int32_t width) {
    return width == 0 ? 2 : width == 1 ? 4 : 1;
}

}

extern "C" {

Icu::CodePointTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t width,
    const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* status) {
    if (!status) throw std::invalid_argument("ucptrie_openFromBinary: null status");
    if (*status > 0) return nullptr;
    if (length <= 0 || (reinterpret_cast<std::uintptr_t>(data) & 3) || type < -1 || type > 1 || width < -1 || width > 2) {
        *status = 1;
        return nullptr;
    }
    if (length < 16) {
        *status = 3;
        return nullptr;
    }
    if (!data) throw std::invalid_argument("ucptrie_openFromBinary: null data");
    const auto& header = *static_cast<const BinaryHeader*>(data);
    const auto actualType = (header.options >> 6) & 3;
    const auto actualWidth = header.options & 7;
    if (header.signature != 0x54726933 || actualType > 1 || actualWidth > 2 || (header.options & 0x38) ||
        (type >= 0 && type != actualType) || (width >= 0 && width != actualWidth)) {
        *status = 3;
        return nullptr;
    }
    Trie trie{};
    trie.indexLength = header.indexLength;
    trie.dataLength = ((header.options & 0xf000) << 4) | header.dataLength;
    trie.index3NullOffset = header.index3NullOffset;
    trie.dataNullOffset = ((header.options & 0xf00) << 8) | header.dataNullOffset;
    trie.highStart = static_cast<std::int32_t>(header.shiftedHighStart) << 9;
    trie.shifted12HighStart = (trie.highStart + 0xfff) >> 12;
    trie.type = actualType;
    trie.valueWidth = actualWidth;
    const auto required = 16 + trie.indexLength * 2 + trie.dataLength * Width(actualWidth);
    if (length < required) {
        *status = 3;
        return nullptr;
    }
    trie.index = reinterpret_cast<const std::uint16_t*>(static_cast<const std::uint8_t*>(data) + 16);
    trie.data.pointer = trie.index + trie.indexLength;
    auto nullOffset = trie.dataNullOffset;
    if (nullOffset >= trie.dataLength) nullOffset = trie.dataLength - 2;
    trie.nullValue = Value(trie, nullOffset);
    auto* result = static_cast<Trie*>(Allocate(sizeof(Trie)));
    if (!result) {
        *status = 7;
        return nullptr;
    }
    *result = trie;
    if (actualLength) *actualLength = required;
    return result;
}

void APS5_VABI ucptrie_close_nid_postfix(Icu::CodePointTrie* trie) {
    Release(trie);
}

std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const Icu::CodePointTrie* trie) {
    return Checked(trie).type;
}

std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const Icu::CodePointTrie* trie) {
    return Checked(trie).valueWidth;
}

std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t character) {
    return SmallIndex(Checked(trie), character);
}

std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t lead,
    std::uint8_t second, std::uint8_t third) {
    if (lead < 0 || lead > (std::numeric_limits<std::int32_t>::max() >> 12))
        throw std::invalid_argument("ucptrie_internalSmallU8Index: overflowing lead");
    const auto character = (lead << 12) | (static_cast<std::int32_t>(second) << 6) | third;
    const auto& value = Checked(trie);
    return character >= value.highStart ? value.dataLength - 2 : SmallIndex(value, character);
}

std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t character,
    const std::uint8_t* start, const std::uint8_t* source) {
    if (!start || !source || source < start) throw std::invalid_argument("ucptrie_internalU8PrevIndex: invalid bounds");
    const auto length = static_cast<std::int32_t>(std::min<std::ptrdiff_t>(source - start, 7));
    auto offset = length;
    character = utf8_prevCharSafeBody_nid_postfix(source - length, 0, &offset, character, -1);
    const auto index = CodePointIndex(Checked(trie), character, 0xffff);
    if (index < 0 || index > (std::numeric_limits<std::int32_t>::max() >> 3))
        throw std::invalid_argument("ucptrie_internalU8PrevIndex: overflowing index");
    return (index << 3) | (length - offset);
}

std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const Icu::CodePointTrie* pointer, std::int32_t character) {
    const auto& trie = Checked(pointer);
    const auto index = static_cast<std::uint32_t>(character) <= 127 ? character :
        CodePointIndex(trie, character, trie.type == 0 ? 0xffff : 0xfff);
    return Value(trie, index);
}

std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t character) {
    return ucptrie_get_nid_postfix(trie, character);
}

std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t start,
    std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointFilter filter, const void* context, std::uint32_t* output) {
    if (option == 0) return Range(trie, start, filter, context, output);
    std::uint32_t temporary;
    if (!output) output = &temporary;
    const auto surrogateEnd = option == 2 ? 0xdfff : 0xdbff;
    const auto end = Range(trie, start, filter, context, output);
    if (end < 0xd7ff || start > surrogateEnd) return end;
    if (*output == surrogateValue) {
        if (end >= surrogateEnd) return end;
    } else {
        if (start <= 0xd7ff) return 0xd7ff;
        *output = surrogateValue;
        if (end > surrogateEnd) return surrogateEnd;
    }
    std::uint32_t nextValue;
    const auto nextEnd = Range(trie, surrogateEnd + 1, filter, context, &nextValue);
    return nextValue == surrogateValue ? nextEnd : surrogateEnd;
}

std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t start,
    std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointFilter filter, const void* context, std::uint32_t* output) {
    return ucptrie_getRange_nid_postfix(trie, start, option, surrogateValue, filter, context, output);
}

std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const Icu::CodePointTrie* pointer, void* data,
    std::int32_t capacity, std::int32_t* status) {
    if (!status) throw std::invalid_argument("ucptrie_toBinary: null status");
    if (*status > 0) return 0;
    const auto& trie = Checked(pointer);
    if (trie.type < 0 || trie.type > 1 || trie.valueWidth < 0 || trie.valueWidth > 2 || capacity < 0 ||
        (capacity > 0 && (!data || (reinterpret_cast<std::uintptr_t>(data) & 3)))) {
        *status = 1;
        return 0;
    }
    const auto indexBytes = static_cast<std::int64_t>(trie.indexLength) * 2;
    const auto dataBytes = static_cast<std::int64_t>(trie.dataLength) * Width(trie.valueWidth);
    const auto total = 16 + indexBytes + dataBytes;
    if (indexBytes < 0 || dataBytes < 0 || total > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ucptrie_toBinary: invalid lengths");
    const auto length = static_cast<std::int32_t>(total);
    if (capacity < length) {
        *status = 15;
        return length;
    }
    if (!data || !trie.index || !trie.data.pointer) throw std::invalid_argument("ucptrie_toBinary: null buffer");
    BinaryHeader header{};
    header.signature = 0x54726933;
    header.options = ((trie.dataLength & 0xf0000) >> 4) | ((trie.dataNullOffset & 0xf0000) >> 8) |
        (trie.type << 6) | trie.valueWidth;
    header.indexLength = trie.indexLength;
    header.dataLength = trie.dataLength;
    header.index3NullOffset = trie.index3NullOffset;
    header.dataNullOffset = trie.dataNullOffset;
    header.shiftedHighStart = trie.highStart >> 9;
    auto* bytes = static_cast<std::uint8_t*>(data);
    std::memcpy(bytes, &header, sizeof(header));
    std::memcpy(bytes + 16, trie.index, indexBytes);
    std::memcpy(bytes + 16 + indexBytes, trie.data.pointer, dataBytes);
    return length;
}

}
