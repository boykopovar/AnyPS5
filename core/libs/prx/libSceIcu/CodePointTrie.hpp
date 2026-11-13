// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

union CodePointTrieData {
    const void* pointer;
    const std::uint16_t* words;
    const std::uint32_t* dwords;
    const std::uint8_t* bytes;
};

struct CodePointTrie {
    const std::uint16_t* index;
    CodePointTrieData data;
    std::int32_t indexLength;
    std::int32_t dataLength;
    std::int32_t highStart;
    std::uint16_t shifted12HighStart;
    std::int8_t type;
    std::int8_t valueWidth;
    std::uint32_t reserved32;
    std::uint16_t reserved16;
    std::uint16_t index3NullOffset;
    std::int32_t dataNullOffset;
    std::uint32_t nullValue;
};

using CodePointFilter = std::uint32_t (APS5_VABI*)(const void*, std::uint32_t);

static_assert(sizeof(CodePointTrieData) == 8 && alignof(CodePointTrieData) == 8);
static_assert(sizeof(CodePointTrie) == 48 && alignof(CodePointTrie) == 8);
static_assert(offsetof(CodePointTrie, index) == 0 && offsetof(CodePointTrie, data) == 8);
static_assert(offsetof(CodePointTrie, indexLength) == 16 && offsetof(CodePointTrie, dataLength) == 20);
static_assert(offsetof(CodePointTrie, highStart) == 24 && offsetof(CodePointTrie, shifted12HighStart) == 28);
static_assert(offsetof(CodePointTrie, type) == 30 && offsetof(CodePointTrie, valueWidth) == 31);
static_assert(offsetof(CodePointTrie, reserved32) == 32 && offsetof(CodePointTrie, reserved16) == 36);
static_assert(offsetof(CodePointTrie, index3NullOffset) == 38 && offsetof(CodePointTrie, dataNullOffset) == 40);
static_assert(offsetof(CodePointTrie, nullValue) == 44);

}

extern "C" {
Icu::CodePointTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t, std::int32_t, const void*, std::int32_t, std::int32_t*, std::int32_t*);
void APS5_VABI ucptrie_close_nid_postfix(Icu::CodePointTrie*);
std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const Icu::CodePointTrie*);
std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const Icu::CodePointTrie*);
std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const Icu::CodePointTrie*, std::int32_t);
std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const Icu::CodePointTrie*, std::int32_t);
std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const Icu::CodePointTrie*, std::int32_t);
std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const Icu::CodePointTrie*, std::int32_t, std::uint8_t, std::uint8_t);
std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const Icu::CodePointTrie*, std::int32_t, const std::uint8_t*, const std::uint8_t*);
std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const Icu::CodePointTrie*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointFilter, const void*, std::uint32_t*);
std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const Icu::CodePointTrie*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointFilter, const void*, std::uint32_t*);
std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const Icu::CodePointTrie*, void*, std::int32_t, std::int32_t*);
}
