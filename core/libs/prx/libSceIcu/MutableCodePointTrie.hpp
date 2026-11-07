// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "CodePointTrie.hpp"

namespace Icu {

struct MutableCodePointTrie {
    std::uint32_t* index;
    std::int32_t indexCapacity;
    std::int32_t index3NullOffset;
    std::uint32_t* data;
    std::int32_t dataCapacity;
    std::int32_t dataLength;
    std::int32_t dataNullOffset;
    std::uint32_t originalInitialValue;
    std::uint32_t initialValue;
    std::uint32_t errorValue;
    std::int32_t highStart;
    std::uint32_t highValue;
    std::uint16_t* index16;
    std::uint8_t flags[0x11000];
};

static_assert(sizeof(MutableCodePointTrie) == 69696 && alignof(MutableCodePointTrie) == 8);
static_assert(offsetof(MutableCodePointTrie, index) == 0 && offsetof(MutableCodePointTrie, indexCapacity) == 8);
static_assert(offsetof(MutableCodePointTrie, index3NullOffset) == 12 && offsetof(MutableCodePointTrie, data) == 16);
static_assert(offsetof(MutableCodePointTrie, dataCapacity) == 24 && offsetof(MutableCodePointTrie, dataLength) == 28);
static_assert(offsetof(MutableCodePointTrie, dataNullOffset) == 32 && offsetof(MutableCodePointTrie, originalInitialValue) == 36);
static_assert(offsetof(MutableCodePointTrie, initialValue) == 40 && offsetof(MutableCodePointTrie, errorValue) == 44);
static_assert(offsetof(MutableCodePointTrie, highStart) == 48 && offsetof(MutableCodePointTrie, highValue) == 52);
static_assert(offsetof(MutableCodePointTrie, index16) == 56 && offsetof(MutableCodePointTrie, flags) == 64);

}

extern "C" {
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t, std::uint32_t, std::int32_t*);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const Icu::MutableCodePointTrie*, std::int32_t*);
void APS5_VABI umutablecptrie_close_nid_postfix(Icu::MutableCodePointTrie*);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const Icu::CodePointTrie*, std::int32_t*);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const Icu::CodePointTrie*, std::int32_t*);
std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const Icu::MutableCodePointTrie*, std::int32_t);
std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const Icu::MutableCodePointTrie*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointFilter, const void*, std::uint32_t*);
void APS5_VABI umutablecptrie_set_nid_postfix(Icu::MutableCodePointTrie*, std::int32_t, std::uint32_t, std::int32_t*);
void APS5_VABI umutablecptrie_setRange_nid_postfix(Icu::MutableCodePointTrie*, std::int32_t, std::int32_t, std::uint32_t, std::int32_t*);
}
