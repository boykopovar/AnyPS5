#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct Trie2;
using Trie2EnumValue = std::uint32_t (APS5_VABI*)(const void*, std::uint32_t);
using Trie2EnumRange = std::int8_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::uint32_t);
} // namespace Icu

extern "C" {
Icu::Trie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t valueBits, const void* data, std::int32_t length,
                                                             std::int32_t* actualLength, std::int32_t* error);
Icu::Trie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t valueBits, std::uint32_t initialValue, std::uint32_t errorValue,
                                                    std::int32_t* error);
void APS5_VABI utrie2_close_nid_postfix(Icu::Trie2* trie);
std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const Icu::Trie2* trie);
std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint);
std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const Icu::Trie2* trie, std::int32_t codeUnit);
std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const Icu::Trie2* trie, void* data, std::int32_t capacity, std::int32_t* error);
void APS5_VABI utrie2_enum_nid_postfix(const Icu::Trie2* trie, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange,
                                       const void* context);
void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const Icu::Trie2* trie, std::int32_t lead, Icu::Trie2EnumValue enumValue,
                                                       Icu::Trie2EnumRange enumRange, const void* context);
std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint,
                                                              const std::uint8_t* source, const std::uint8_t* limit);
std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint,
                                                              const std::uint8_t* start, const std::uint8_t* source);
}
