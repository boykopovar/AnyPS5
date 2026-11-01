// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

using Trie = Icu::MutableCodePointTrie;

void* Allocate(std::size_t size) {
    return std::malloc(size);
}

bool Failed(std::int32_t* status) {
    if (!status) throw std::invalid_argument("UMutableCPTrie: null status");
    return *status > 0;
}

const Trie& Checked(const Trie* trie) {
    if (!trie) throw std::invalid_argument("UMutableCPTrie: null trie");
    return *trie;
}

void Close(Trie* trie) {
    if (!trie) return;
    std::free(trie->index);
    std::free(trie->data);
    std::free(trie->index16);
    std::free(trie);
}

Trie* Create(std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* status) {
    auto* trie = static_cast<Trie*>(Allocate(sizeof(Trie)));
    if (!trie) {
        *status = 7;
        return nullptr;
    }
    trie->index = static_cast<std::uint32_t*>(Allocate(4096 * 4));
    trie->indexCapacity = 0;
    trie->index3NullOffset = -1;
    trie->data = static_cast<std::uint32_t*>(Allocate(16384 * 4));
    trie->dataCapacity = 0;
    trie->dataLength = 0;
    trie->dataNullOffset = -1;
    trie->originalInitialValue = trie->initialValue = trie->highValue = initialValue;
    trie->errorValue = errorValue;
    trie->highStart = 0;
    trie->index16 = nullptr;
    if (!trie->index || !trie->data) {
        *status = 7;
        Close(trie);
        return nullptr;
    }
    trie->indexCapacity = 4096;
    trie->dataCapacity = 16384;
    return trie;
}

std::uint32_t Index(const Trie& trie, std::int32_t offset) {
    if (!trie.index || offset < 0 || offset >= trie.indexCapacity || offset >= 0x11000)
        throw std::invalid_argument("UMutableCPTrie: invalid index access");
    return trie.index[offset];
}

std::uint32_t Value(const Trie& trie, std::uint64_t offset) {
    if (!trie.data || offset >= static_cast<std::uint64_t>(trie.dataLength))
        throw std::invalid_argument("UMutableCPTrie: invalid data access");
    return trie.data[offset];
}

bool Extend(Trie& trie, std::int32_t character) {
    if (character < trie.highStart) return true;
    const auto limit = (character + 512) & ~511;
    const auto used = trie.highStart >> 4;
    const auto needed = limit >> 4;
    if (used < 0 || needed > 0x11000 || used > trie.indexCapacity)
        throw std::invalid_argument("UMutableCPTrie: invalid high start");
    if (needed > trie.indexCapacity) {
        auto* index = static_cast<std::uint32_t*>(Allocate(0x11000 * 4));
        if (!index) return false;
        std::memcpy(index, trie.index, used * 4);
        std::free(trie.index);
        trie.index = index;
        trie.indexCapacity = 0x11000;
    }
    std::fill(trie.flags + used, trie.flags + needed, 0);
    std::fill(trie.index + used, trie.index + needed, trie.initialValue);
    trie.highStart = limit;
    return true;
}

std::int32_t AllocateBlock(Trie& trie, std::int32_t length) {
    const auto start = trie.dataLength;
    if (start < 0 || start > 0x110000 - length) return -1;
    if (start + length > trie.dataCapacity) {
        const auto capacity = trie.dataCapacity < 0x20000 ? 0x20000 : trie.dataCapacity < 0x110000 ? 0x110000 : 0;
        if (!capacity) return -1;
        auto* data = static_cast<std::uint32_t*>(Allocate(capacity * 4));
        if (!data) return -1;
        std::memcpy(data, trie.data, static_cast<std::size_t>(start) * 4);
        std::free(trie.data);
        trie.data = data;
        trie.dataCapacity = capacity;
    }
    trie.dataLength = start + length;
    return start;
}

std::int32_t WritableBlock(Trie& trie, std::int32_t offset) {
    const auto current = Index(trie, offset);
    if (trie.flags[offset] == 1) return static_cast<std::int32_t>(current);
    const bool bmp = offset < 4096;
    auto block = AllocateBlock(trie, bmp ? 64 : 16);
    if (block < 0) return -1;
    const auto first = bmp ? offset & ~3 : offset;
    const auto last = bmp ? first + 4 : first + 1;
    for (auto entry = first; entry < last; ++entry, block += 16) {
        std::fill_n(trie.data + block, 16, Index(trie, entry));
        trie.flags[entry] = 1;
        trie.index[entry] = block;
    }
    return trie.index[offset];
}

std::int32_t Range(const Trie* pointer, std::int32_t start, Icu::CodePointFilter filter,
                   const void* context, std::uint32_t* output) {
    if (static_cast<std::uint32_t>(start) > 0x10ffff) return -1;
    const auto& trie = Checked(pointer);
    if (start >= trie.highStart) {
        if (output) *output = filter ? filter(context, trie.highValue) : trie.highValue;
        return 0x10ffff;
    }
    const auto nullValue = filter ? filter(context, trie.initialValue) : trie.initialValue;
    const auto filtered = [&](std::uint32_t value) {
        return value == trie.initialValue ? nullValue : filter ? filter(context, value) : value;
    };
    auto character = start;
    bool haveValue = false;
    std::uint32_t rawValue = 0;
    std::uint32_t value = 0;
    const auto accept = [&](std::uint32_t raw) {
        if (!haveValue) {
            rawValue = raw;
            value = filtered(raw);
            if (output) *output = value;
            haveValue = true;
        } else if (raw != rawValue) {
            if (!filter || filtered(raw) != value) return false;
            rawValue = raw;
        }
        return true;
    };
    do {
        const auto entry = character >> 4;
        const auto block = Index(trie, entry);
        if (trie.flags[entry] == 0) {
            if (!accept(block)) return character - 1;
            character = (character + 16) & ~15;
        } else {
            auto position = static_cast<std::uint64_t>(block) + (character & 15);
            if (!accept(Value(trie, position))) return character - 1;
            while ((++character & 15) != 0) {
                if (!accept(Value(trie, ++position))) return character - 1;
            }
        }
    } while (character < trie.highStart);
    return filtered(trie.highValue) == value ? 0x10ffff : character - 1;
}

Trie* From(const Icu::CodePointTrie* source, bool direct, std::int32_t* status) {
    if (!source) {
        *status = 1;
        return nullptr;
    }
    const auto read = [&](std::int32_t offset) -> std::uint32_t {
        if (!source->data.pointer || offset < 0 || offset >= source->dataLength)
            throw std::invalid_argument("umutablecptrie_fromUCPTrie: invalid data access");
        if (source->valueWidth == 0) return source->data.words[offset];
        if (source->valueWidth == 1) return source->data.dwords[offset];
        return source->data.bytes[offset];
    };
    const auto initialValue = direct ? read(source->dataLength - 2) : ucptrie_get_nid_postfix(source, 0x10ffff);
    const auto errorValue = direct ? read(source->dataLength - 1) : ucptrie_get_nid_postfix(source, -1);
    auto* result = Create(initialValue, errorValue, status);
    if (!result) return nullptr;
    try {
        std::uint32_t value;
        std::int32_t start = 0;
        std::int32_t end;
        while ((end = ucptrie_getRange_nid_postfix(source, start, 0, 0, nullptr, nullptr, &value)) >= 0) {
            if (value != result->initialValue) {
                if (start == end) umutablecptrie_set_nid_postfix(result, start, value, status);
                else umutablecptrie_setRange_nid_postfix(result, start, end, value, status);
            }
            start = end + 1;
        }
    } catch (...) {
        Close(result);
        throw;
    }
    if (*status > 0) {
        Close(result);
        return nullptr;
    }
    return result;
}

}

extern "C" {

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue,
    std::uint32_t errorValue, std::int32_t* status) {
    return Failed(status) ? nullptr : Create(initialValue, errorValue, status);
}

void APS5_VABI umutablecptrie_close_nid_postfix(Icu::MutableCodePointTrie* trie) {
    Close(trie);
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const Icu::MutableCodePointTrie* source,
    std::int32_t* status) {
    if (Failed(status) || !source) return nullptr;
    const auto indexCapacity = source->highStart <= 0x10000 ? 4096 : 0x11000;
    const auto used = source->highStart >> 4;
    if (used < 0 || used > indexCapacity || used > source->indexCapacity || source->dataLength < 0 ||
        source->dataLength > source->dataCapacity || source->dataCapacity < 0 || source->dataCapacity > 0x110000 ||
        !source->index || !source->data)
        throw std::invalid_argument("umutablecptrie_clone: invalid arrays");
    auto* clone = static_cast<Trie*>(Allocate(sizeof(Trie)));
    if (!clone) {
        *status = 7;
        return nullptr;
    }
    clone->index = static_cast<std::uint32_t*>(Allocate(indexCapacity * 4));
    clone->indexCapacity = indexCapacity;
    clone->index3NullOffset = source->index3NullOffset;
    clone->data = static_cast<std::uint32_t*>(Allocate(source->dataCapacity * 4));
    clone->dataCapacity = source->dataCapacity;
    clone->dataLength = source->dataLength;
    clone->dataNullOffset = source->dataNullOffset;
    clone->originalInitialValue = source->originalInitialValue;
    clone->initialValue = source->initialValue;
    clone->errorValue = source->errorValue;
    clone->highStart = source->highStart;
    clone->highValue = source->highValue;
    clone->index16 = nullptr;
    if (!clone->index || !clone->data) {
        *status = 7;
        Close(clone);
        return nullptr;
    }
    std::memcpy(clone->flags, source->flags, used);
    std::memcpy(clone->index, source->index, used * 4);
    std::memcpy(clone->data, source->data, static_cast<std::size_t>(source->dataLength) * 4);
    return clone;
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const Icu::CodePointTrie* source,
    std::int32_t* status) {
    return Failed(status) ? nullptr : From(source, false, status);
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const Icu::CodePointTrie* source,
    std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (source && (source->valueWidth < 0 || source->valueWidth > 2)) {
        *status = 1;
        return nullptr;
    }
    return From(source, true, status);
}

std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const Icu::MutableCodePointTrie* pointer, std::int32_t character) {
    const auto& trie = Checked(pointer);
    if (static_cast<std::uint32_t>(character) > 0x10ffff) return trie.errorValue;
    if (character >= trie.highStart) return trie.highValue;
    const auto offset = character >> 4;
    const auto index = Index(trie, offset);
    return trie.flags[offset] == 0 ? index : Value(trie, static_cast<std::uint64_t>(index) + (character & 15));
}

std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const Icu::MutableCodePointTrie* trie, std::int32_t start,
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

void APS5_VABI umutablecptrie_set_nid_postfix(Icu::MutableCodePointTrie* pointer, std::int32_t character,
    std::uint32_t value, std::int32_t* status) {
    if (Failed(status)) return;
    auto& trie = const_cast<Trie&>(Checked(pointer));
    if (static_cast<std::uint32_t>(character) > 0x10ffff) {
        *status = 1;
        return;
    }
    if (!Extend(trie, character)) {
        *status = 7;
        return;
    }
    const auto block = WritableBlock(trie, character >> 4);
    if (block < 0) *status = 7;
    else trie.data[block + (character & 15)] = value;
}

void APS5_VABI umutablecptrie_setRange_nid_postfix(Icu::MutableCodePointTrie* pointer, std::int32_t start,
    std::int32_t end, std::uint32_t value, std::int32_t* status) {
    if (Failed(status)) return;
    auto& trie = const_cast<Trie&>(Checked(pointer));
    if (static_cast<std::uint32_t>(start) > 0x10ffff || static_cast<std::uint32_t>(end) > 0x10ffff || start > end) {
        *status = 1;
        return;
    }
    if (!Extend(trie, end)) {
        *status = 7;
        return;
    }
    auto limit = end + 1;
    if (start & 15) {
        const auto block = WritableBlock(trie, start >> 4);
        if (block < 0) {
            *status = 7;
            return;
        }
        const auto next = (start + 15) & ~15;
        const auto count = next <= limit ? 16 : limit & 15;
        std::fill(trie.data + block + (start & 15), trie.data + block + count, value);
        if (next > limit) return;
        start = next;
    }
    const auto rest = limit & 15;
    limit &= ~15;
    for (; start < limit; start += 16) {
        const auto entry = start >> 4;
        if (trie.flags[entry] == 0) trie.index[entry] = value;
        else std::fill_n(trie.data + Index(trie, entry), 16, value);
    }
    if (rest) {
        const auto block = WritableBlock(trie, start >> 4);
        if (block < 0) *status = 7;
        else std::fill_n(trie.data + block, rest, value);
    }
}

}
