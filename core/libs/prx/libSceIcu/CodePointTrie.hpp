#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct CodePointTrie;
struct CodePointMap;
struct MutableCodePointTrie;
using CodePointMapValueFilter = std::uint32_t (APS5_VABI*)(const void*, std::uint32_t);
} // namespace Icu

extern "C" {
Icu::CodePointTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t valueWidth, const void* data,
                                                                  std::int32_t length, std::int32_t* actualLength,
                                                                  std::int32_t* error);
void APS5_VABI ucptrie_close_nid_postfix(Icu::CodePointTrie* trie);
std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const Icu::CodePointTrie* trie);
std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const Icu::CodePointTrie* trie);
std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint);
std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t start, std::int32_t option,
                                                    std::uint32_t surrogateValue, Icu::CodePointMapValueFilter filter,
                                                    const void* context, std::uint32_t* value);
std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const Icu::CodePointTrie* trie, void* data, std::int32_t capacity,
                                                    std::int32_t* error);
std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint);
std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t lead,
                                                                std::uint8_t second, std::uint8_t third);
std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint,
                                                               const std::uint8_t* start, const std::uint8_t* source);
std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const Icu::CodePointMap* map, std::int32_t codePoint);
std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const Icu::CodePointMap* map, std::int32_t start, std::int32_t option,
                                                   std::uint32_t surrogateValue, Icu::CodePointMapValueFilter filter,
                                                   const void* context, std::uint32_t* value);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue, std::uint32_t errorValue,
                                                                      std::int32_t* error);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const Icu::MutableCodePointTrie* other, std::int32_t* error);
void APS5_VABI umutablecptrie_close_nid_postfix(Icu::MutableCodePointTrie* trie);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const Icu::CodePointMap* map, std::int32_t* error);
Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t* error);
std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const Icu::MutableCodePointTrie* trie, std::int32_t codePoint);
std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const Icu::MutableCodePointTrie* trie, std::int32_t start,
                                                           std::int32_t option, std::uint32_t surrogateValue,
                                                           Icu::CodePointMapValueFilter filter, const void* context,
                                                           std::uint32_t* value);
void APS5_VABI umutablecptrie_set_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t codePoint, std::uint32_t value,
                                              std::int32_t* error);
void APS5_VABI umutablecptrie_setRange_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t start, std::int32_t end,
                                                   std::uint32_t value, std::int32_t* error);
Icu::CodePointTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t type,
                                                                         std::int32_t valueWidth, std::int32_t* error);
}
