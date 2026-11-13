// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using Trie = Icu::Trie2;

struct Header {
    std::uint32_t signature;
    std::uint16_t options;
    std::uint16_t indexLength;
    std::uint16_t shiftedDataLength;
    std::uint16_t index2NullOffset;
    std::uint16_t dataNullOffset;
    std::uint16_t shiftedHighStart;
};

static_assert(sizeof(Header) == 16 && alignof(Header) == 4);
static_assert(offsetof(Header, options) == 4 && offsetof(Header, shiftedHighStart) == 14);

void* Allocate(std::size_t size) {
    return std::malloc(size);
}

bool Failed(std::int32_t* status) {
    if (!status) throw std::invalid_argument("UTrie2: null status");
    return *status > 0;
}

const Trie& Checked(const Trie* trie) {
    if (!trie) throw std::invalid_argument("UTrie2: null trie");
    return *trie;
}

bool IsLead(std::int32_t character) {
    return (static_cast<std::uint32_t>(character) & 0xfffffc00) == 0xd800;
}

std::uint16_t Index(const Trie& trie, std::int32_t offset) {
    if (!trie.index || offset < 0 || offset >= trie.indexLength)
        throw std::invalid_argument("UTrie2: invalid index access");
    return trie.index[offset];
}

std::uint32_t FrozenValue(const Trie& trie, std::int32_t offset, bool words) {
    if (words) {
        if (!trie.index || offset < 0 || static_cast<std::int64_t>(offset) >= static_cast<std::int64_t>(trie.indexLength) + trie.dataLength)
            throw std::invalid_argument("UTrie2: invalid 16-bit data access");
        return trie.index[offset];
    }
    if (!trie.data32 || offset < 0 || offset >= trie.dataLength || (reinterpret_cast<std::uintptr_t>(trie.data32) & 3))
        throw std::invalid_argument("UTrie2: invalid 32-bit data access");
    return trie.data32[offset];
}

std::int32_t CodePointIndex(const Trie& trie, std::int32_t character, std::int32_t asciiOffset) {
    if (static_cast<std::uint32_t>(character) <= 0xffff) {
        const auto offset = IsLead(character) ? 2048 - (0xd800 >> 5) : 0;
        return (static_cast<std::int32_t>(Index(trie, offset + (character >> 5))) << 2) + (character & 31);
    }
    if (static_cast<std::uint32_t>(character) > 0x10ffff) return asciiOffset + 128;
    if (character >= trie.highStart) return trie.highValueIndex;
    const auto second = Index(trie, 2080 + (character >> 11)) + ((character >> 5) & 63);
    return (static_cast<std::int32_t>(Index(trie, second)) << 2) + (character & 31);
}

std::uint32_t BuilderValue(const Icu::NewTrie2* builder, std::int32_t character, bool codePoint) {
    if (!builder || !builder->data) throw std::invalid_argument("UTrie2: null builder data");
    std::int64_t offset;
    if (character >= builder->highStart && (!IsLead(character) || codePoint)) {
        offset = static_cast<std::int64_t>(builder->dataLength) - 4;
    } else {
        auto second = IsLead(character) && codePoint ? 2048 - (0xd800 >> 5) + (character >> 5) :
            static_cast<std::int64_t>(builder->index1[character >> 11]) + ((character >> 5) & 63);
        if (second < 0 || second >= 35488) throw std::invalid_argument("UTrie2: invalid builder index");
        offset = static_cast<std::int64_t>(builder->index2[second]) + (character & 31);
    }
    if (offset < 0 || offset >= builder->dataLength) throw std::invalid_argument("UTrie2: invalid builder data offset");
    return builder->data[offset];
}

std::int32_t PackedIndex(const Trie& trie, std::int32_t character, std::int32_t consumed) {
    const auto index = CodePointIndex(trie, character, trie.data32 ? 0 : trie.indexLength);
    if (index < 0 || index > (std::numeric_limits<std::int32_t>::max() >> 3))
        throw std::invalid_argument("UTrie2: overflowing UTF-8 index");
    return (index << 3) | consumed;
}

}

extern "C" {

Icu::Trie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t width, const void* data,
    std::int32_t length, std::int32_t* actualLength, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (length <= 0 || (reinterpret_cast<std::uintptr_t>(data) & 3) || width < 0 || width > 1) {
        *status = 1;
        return nullptr;
    }
    if (length < 16) {
        *status = 3;
        return nullptr;
    }
    if (!data) throw std::invalid_argument("utrie2_openFromSerialized: null data");
    const auto& header = *static_cast<const Header*>(data);
    if (header.signature != 0x54726932 || (header.options & 15) != width) {
        *status = 3;
        return nullptr;
    }
    Trie trie{};
    trie.indexLength = header.indexLength;
    trie.dataLength = static_cast<std::int32_t>(header.shiftedDataLength) << 2;
    trie.index2NullOffset = header.index2NullOffset;
    trie.dataNullOffset = header.dataNullOffset;
    trie.highStart = static_cast<std::int32_t>(header.shiftedHighStart) << 11;
    trie.highValueIndex = trie.dataLength - 4 + (width == 0 ? trie.indexLength : 0);
    trie.length = 16 + trie.indexLength * 2 + trie.dataLength * (width == 0 ? 2 : 4);
    if (length < trie.length) {
        *status = 3;
        return nullptr;
    }
    trie.memory = const_cast<void*>(data);
    trie.index = reinterpret_cast<const std::uint16_t*>(static_cast<const std::uint8_t*>(data) + 16);
    if (width == 0) {
        trie.data16 = trie.index + trie.indexLength;
        trie.initialValue = FrozenValue(trie, trie.dataNullOffset, true);
        if (trie.dataLength <= 128) throw std::invalid_argument("UTrie2: missing error value");
        trie.errorValue = trie.data16[128];
    } else {
        trie.data32 = reinterpret_cast<const std::uint32_t*>(trie.index + trie.indexLength);
        trie.initialValue = FrozenValue(trie, trie.dataNullOffset, false);
        trie.errorValue = FrozenValue(trie, 128, false);
    }
    auto* result = static_cast<Trie*>(Allocate(sizeof(Trie)));
    if (!result) {
        *status = 7;
        return nullptr;
    }
    *result = trie;
    if (actualLength) *actualLength = trie.length;
    return result;
}

Icu::Trie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t width, std::uint32_t initialValue,
    std::uint32_t errorValue, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (width < 0 || width > 1) {
        *status = 1;
        return nullptr;
    }
    auto* trie = static_cast<Trie*>(Allocate(sizeof(Trie)));
    if (!trie) {
        *status = 7;
        return nullptr;
    }
    *trie = {};
    trie->indexLength = 2112;
    trie->dataLength = 196;
    trie->length = 16 + 2112 * 2 + 196 * (width == 0 ? 2 : 4);
    trie->memory = Allocate(trie->length);
    if (!trie->memory) {
        std::free(trie);
        *status = 7;
        return nullptr;
    }
    trie->isMemoryOwned = 1;
    const auto move = width == 0 ? 2112 : 0;
    trie->dataNullOffset = move;
    trie->initialValue = initialValue;
    trie->errorValue = errorValue;
    trie->highValueIndex = move + 192;
    const Header header{0x54726932, static_cast<std::uint16_t>(width), 2112, 49, 0, static_cast<std::uint16_t>(move), 0};
    std::memcpy(trie->memory, &header, sizeof(header));
    auto* index = reinterpret_cast<std::uint16_t*>(static_cast<std::uint8_t*>(trie->memory) + 16);
    trie->index = index;
    std::fill_n(index, 2080, move >> 2);
    std::fill_n(index + 2080, 2, move + 128);
    std::fill(index + 2082, index + 2112, move);
    if (width == 0) {
        auto* values = index + 2112;
        trie->data16 = values;
        for (int offset = 0; offset < 196; ++offset)
            values[offset] = static_cast<std::uint16_t>(offset >= 128 && offset < 192 ? errorValue : initialValue);
    } else {
        auto* values = reinterpret_cast<std::uint32_t*>(index + 2112);
        trie->data32 = values;
        for (int offset = 0; offset < 196; ++offset)
            values[offset] = offset >= 128 && offset < 192 ? errorValue : initialValue;
    }
    return trie;
}

void APS5_VABI utrie2_close_nid_postfix(Icu::Trie2* trie) {
    if (!trie) return;
    if (trie->isMemoryOwned) std::free(trie->memory);
    if (trie->builder) {
        std::free(trie->builder->data);
        std::free(trie->builder);
    }
    std::free(trie);
}

std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const Icu::Trie2* trie) {
    return Checked(trie).builder == nullptr;
}

std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const Icu::Trie2* pointer, std::int32_t character) {
    const auto& trie = Checked(pointer);
    if (trie.data16) return FrozenValue(trie, CodePointIndex(trie, character, trie.indexLength), true);
    if (trie.data32) return FrozenValue(trie, CodePointIndex(trie, character, 0), false);
    if (static_cast<std::uint32_t>(character) > 0x10ffff) return trie.errorValue;
    return BuilderValue(trie.builder, character, true);
}

std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const Icu::Trie2* pointer, std::int32_t character) {
    const auto& trie = Checked(pointer);
    if (!IsLead(character)) return trie.errorValue;
    if (trie.data16 || trie.data32) {
        const auto offset = (static_cast<std::int32_t>(Index(trie, character >> 5)) << 2) + (character & 31);
        return FrozenValue(trie, offset, trie.data16 != nullptr);
    }
    return BuilderValue(trie.builder, character, false);
}

std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t character,
    const std::uint8_t* source, const std::uint8_t* limit) {
    if (!source || !limit || limit < source) throw std::invalid_argument("utrie2_internalU8NextIndex: invalid bounds");
    const auto length = static_cast<std::int32_t>(std::min<std::ptrdiff_t>(limit - source, 7));
    std::int32_t offset = 0;
    character = utf8_nextCharSafeBody_nid_postfix(source, &offset, length, character, -1);
    return PackedIndex(Checked(trie), character, offset);
}

std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t character,
    const std::uint8_t* start, const std::uint8_t* source) {
    if (!source || !start || source < start) throw std::invalid_argument("utrie2_internalU8PrevIndex: invalid bounds");
    const auto length = static_cast<std::int32_t>(std::min<std::ptrdiff_t>(source - start, 7));
    auto offset = length;
    character = utf8_prevCharSafeBody_nid_postfix(source - length, 0, &offset, character, -1);
    return PackedIndex(Checked(trie), character, length - offset);
}

std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const Icu::Trie2* trie, void* data,
    std::int32_t capacity, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (!trie || !trie->memory || trie->builder || capacity < 0 ||
        (capacity > 0 && (!data || (reinterpret_cast<std::uintptr_t>(data) & 3)))) {
        *status = 1;
        return 0;
    }
    if (capacity < trie->length) *status = 15;
    else {
        if (!data || trie->length < 0) throw std::invalid_argument("utrie2_serialize: invalid copy");
        std::memcpy(data, trie->memory, trie->length);
    }
    return trie->length;
}

}
