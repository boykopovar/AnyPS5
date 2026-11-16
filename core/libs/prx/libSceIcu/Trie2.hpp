// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct NewTrie2 {
    std::int32_t index1[544];
    std::int32_t index2[35488];
    std::uint32_t* data;
    std::uint32_t initialValue;
    std::uint32_t errorValue;
    std::int32_t index2Length;
    std::int32_t dataCapacity;
    std::int32_t dataLength;
    std::int32_t firstFreeBlock;
    std::int32_t index2NullOffset;
    std::int32_t dataNullOffset;
    std::int32_t highStart;
    std::int8_t isCompacted;
    std::int32_t map[34852];
};

struct Trie2 {
    const std::uint16_t* index;
    const std::uint16_t* data16;
    const std::uint32_t* data32;
    std::int32_t indexLength;
    std::int32_t dataLength;
    std::uint16_t index2NullOffset;
    std::uint16_t dataNullOffset;
    std::uint32_t initialValue;
    std::uint32_t errorValue;
    std::int32_t highStart;
    std::int32_t highValueIndex;
    void* memory;
    std::int32_t length;
    std::int8_t isMemoryOwned;
    std::int8_t padding1;
    std::int16_t padding2;
    NewTrie2* builder;
};

static_assert(sizeof(NewTrie2) == 283584 && alignof(NewTrie2) == 8);
static_assert(offsetof(NewTrie2, index1) == 0 && offsetof(NewTrie2, index2) == 2176);
static_assert(offsetof(NewTrie2, data) == 144128 && offsetof(NewTrie2, initialValue) == 144136);
static_assert(offsetof(NewTrie2, errorValue) == 144140 && offsetof(NewTrie2, index2Length) == 144144);
static_assert(offsetof(NewTrie2, dataCapacity) == 144148 && offsetof(NewTrie2, dataLength) == 144152);
static_assert(offsetof(NewTrie2, firstFreeBlock) == 144156 && offsetof(NewTrie2, index2NullOffset) == 144160);
static_assert(offsetof(NewTrie2, dataNullOffset) == 144164 && offsetof(NewTrie2, highStart) == 144168);
static_assert(offsetof(NewTrie2, isCompacted) == 144172 && offsetof(NewTrie2, map) == 144176);
static_assert(sizeof(Trie2) == 80 && alignof(Trie2) == 8);
static_assert(offsetof(Trie2, index) == 0 && offsetof(Trie2, data16) == 8 && offsetof(Trie2, data32) == 16);
static_assert(offsetof(Trie2, indexLength) == 24 && offsetof(Trie2, dataLength) == 28);
static_assert(offsetof(Trie2, index2NullOffset) == 32 && offsetof(Trie2, dataNullOffset) == 34);
static_assert(offsetof(Trie2, initialValue) == 36 && offsetof(Trie2, errorValue) == 40);
static_assert(offsetof(Trie2, highStart) == 44 && offsetof(Trie2, highValueIndex) == 48);
static_assert(offsetof(Trie2, memory) == 56 && offsetof(Trie2, length) == 64);
static_assert(offsetof(Trie2, isMemoryOwned) == 68 && offsetof(Trie2, padding1) == 69 && offsetof(Trie2, padding2) == 70);
static_assert(offsetof(Trie2, builder) == 72);

}

extern "C" {
Icu::Trie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t, const void*, std::int32_t, std::int32_t*, std::int32_t*);
Icu::Trie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t, std::uint32_t, std::uint32_t, std::int32_t*);
void APS5_VABI utrie2_close_nid_postfix(Icu::Trie2*);
std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const Icu::Trie2*);
std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const Icu::Trie2*, std::int32_t);
std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const Icu::Trie2*, std::int32_t);
std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const Icu::Trie2*, std::int32_t, const std::uint8_t*, const std::uint8_t*);
std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const Icu::Trie2*, std::int32_t, const std::uint8_t*, const std::uint8_t*);
std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const Icu::Trie2*, void*, std::int32_t, std::int32_t*);
}
