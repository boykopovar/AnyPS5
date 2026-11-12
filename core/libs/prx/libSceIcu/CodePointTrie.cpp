#include "prx/libSceIcu/CodePointTrie.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucpmap.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include <cstdint>

namespace {

struct Filter {
    Icu::CodePointMapValueFilter function;
    const void* context;
};

std::uint32_t U_CALLCONV ForwardFilter(const void* context, std::uint32_t value) {
    const auto& filter = *static_cast<const Filter*>(context);
    return filter.function(filter.context, value);
}

const UCPTrie* Native(const Icu::CodePointTrie* trie) {
    return reinterpret_cast<const UCPTrie*>(trie);
}

const UCPMap* Native(const Icu::CodePointMap* map) {
    return reinterpret_cast<const UCPMap*>(map);
}

UMutableCPTrie* Native(Icu::MutableCodePointTrie* trie) {
    return reinterpret_cast<UMutableCPTrie*>(trie);
}

const UMutableCPTrie* Native(const Icu::MutableCodePointTrie* trie) {
    return reinterpret_cast<const UMutableCPTrie*>(trie);
}

Icu::MutableCodePointTrie* Guest(UMutableCPTrie* trie) {
    return reinterpret_cast<Icu::MutableCodePointTrie*>(trie);
}

Icu::CodePointTrie* Guest(UCPTrie* trie) {
    return reinterpret_cast<Icu::CodePointTrie*>(trie);
}

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(error);
}

UCPMapRangeOption Option(std::int32_t option) {
    return static_cast<UCPMapRangeOption>(option);
}

template<typename TRange>
std::int32_t Range(Icu::CodePointMapValueFilter filter, const void* context, const TRange& range) {
    if (!filter) return range(nullptr, nullptr);
    const Filter forward{filter, context};
    return range(ForwardFilter, &forward);
}

}

extern "C" {

Icu::CodePointTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t valueWidth, const void* data,
                                                                  std::int32_t length, std::int32_t* actualLength,
                                                                  std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && length > 0 && !data) APS5_INVALID_ARG_EX;
    return Guest(ucptrie_openFromBinary(static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth), data, length,
                                        actualLength, Status(error)));
}

void APS5_VABI ucptrie_close_nid_postfix(Icu::CodePointTrie* trie) {
    ucptrie_close(reinterpret_cast<UCPTrie*>(trie));
}

std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const Icu::CodePointTrie* trie) {
    if (!trie) APS5_INVALID_ARG_EX;
    return ucptrie_getType(Native(trie));
}

std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const Icu::CodePointTrie* trie) {
    if (!trie) APS5_INVALID_ARG_EX;
    return ucptrie_getValueWidth(Native(trie));
}

std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint) {
    if (!trie) APS5_INVALID_ARG_EX;
    return ucptrie_get(Native(trie), codePoint);
}

std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t start, std::int32_t option,
                                                    std::uint32_t surrogateValue, Icu::CodePointMapValueFilter filter,
                                                    const void* context, std::uint32_t* value) {
    if (!trie) APS5_INVALID_ARG_EX;
    return Range(filter, context, [&](UCPMapValueFilter* forward, const void* forwardContext) {
        return ucptrie_getRange(Native(trie), start, Option(option), surrogateValue, forward, forwardContext, value);
    });
}

std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const Icu::CodePointTrie* trie, void* data, std::int32_t capacity,
                                                    std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && !trie) APS5_INVALID_ARG_EX;
    return ucptrie_toBinary(Native(trie), data, capacity, Status(error));
}

std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint) {
    if (!trie) APS5_INVALID_ARG_EX;
    return ucptrie_internalSmallIndex(Native(trie), codePoint);
}

std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t lead,
                                                                std::uint8_t second, std::uint8_t third) {
    if (!trie) APS5_INVALID_ARG_EX;
    return ucptrie_internalSmallU8Index(Native(trie), lead, second, third);
}

std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t codePoint,
                                                               const std::uint8_t* start, const std::uint8_t* source) {
    if (!trie || !start || !source) APS5_INVALID_ARG_EX;
    return ucptrie_internalU8PrevIndex(Native(trie), codePoint, start, source);
}

std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const Icu::CodePointMap* map, std::int32_t codePoint) {
    if (!map) APS5_INVALID_ARG_EX;
    return ucpmap_get(Native(map), codePoint);
}

std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const Icu::CodePointMap* map, std::int32_t start, std::int32_t option,
                                                   std::uint32_t surrogateValue, Icu::CodePointMapValueFilter filter,
                                                   const void* context, std::uint32_t* value) {
    if (!map) APS5_INVALID_ARG_EX;
    return Range(filter, context, [&](UCPMapValueFilter* forward, const void* forwardContext) {
        return ucpmap_getRange(Native(map), start, Option(option), surrogateValue, forward, forwardContext, value);
    });
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue, std::uint32_t errorValue,
                                                                      std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Guest(umutablecptrie_open(initialValue, errorValue, Status(error)));
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const Icu::MutableCodePointTrie* other, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Guest(umutablecptrie_clone(Native(other), Status(error)));
}

void APS5_VABI umutablecptrie_close_nid_postfix(Icu::MutableCodePointTrie* trie) {
    umutablecptrie_close(Native(trie));
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const Icu::CodePointMap* map, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Guest(umutablecptrie_fromUCPMap(Native(map), Status(error)));
}

Icu::MutableCodePointTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const Icu::CodePointTrie* trie, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Guest(umutablecptrie_fromUCPTrie(Native(trie), Status(error)));
}

std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const Icu::MutableCodePointTrie* trie, std::int32_t codePoint) {
    if (!trie) APS5_INVALID_ARG_EX;
    return umutablecptrie_get(Native(trie), codePoint);
}

std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const Icu::MutableCodePointTrie* trie, std::int32_t start,
                                                           std::int32_t option, std::uint32_t surrogateValue,
                                                           Icu::CodePointMapValueFilter filter, const void* context,
                                                           std::uint32_t* value) {
    if (!trie) APS5_INVALID_ARG_EX;
    return Range(filter, context, [&](UCPMapValueFilter* forward, const void* forwardContext) {
        return umutablecptrie_getRange(Native(trie), start, Option(option), surrogateValue, forward, forwardContext, value);
    });
}

void APS5_VABI umutablecptrie_set_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t codePoint, std::uint32_t value,
                                              std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && !trie) APS5_INVALID_ARG_EX;
    umutablecptrie_set(Native(trie), codePoint, value, Status(error));
}

void APS5_VABI umutablecptrie_setRange_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t start, std::int32_t end,
                                                   std::uint32_t value, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && !trie) APS5_INVALID_ARG_EX;
    umutablecptrie_setRange(Native(trie), start, end, value, Status(error));
}

Icu::CodePointTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(Icu::MutableCodePointTrie* trie, std::int32_t type,
                                                                         std::int32_t valueWidth, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && !trie) APS5_INVALID_ARG_EX;
    return Guest(umutablecptrie_buildImmutable(Native(trie), static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth),
                                               Status(error)));
}

}
