#include "prx/libSceIcu/CodePointTrie.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucpmap.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include <cstdint>

namespace {

struct ValueFilter {
    Icu::CodePointValueFilter filter;
    const void* context;
};

uint32_t U_CALLCONV ForwardValueFilter(const void* context, uint32_t value) {
    const auto& forward = *static_cast<const ValueFilter*>(context);
    return forward.filter(forward.context, value);
}

template <typename T>
T* Required(T* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
    return pointer;
}

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(Required(error));
}

template <typename GetRange, typename Map>
std::int32_t Range(GetRange getRange, const Map* map, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
    Required(map);
    const ValueFilter forward{filter, context};
    return getRange(map, start, static_cast<UCPMapRangeOption>(option), surrogateValue, filter ? ForwardValueFilter : nullptr, filter ? &forward : nullptr, value);
}

}

extern "C" {

std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const UCPMap* map, std::int32_t c) {
    return ucpmap_get(Required(map), c);
}

std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const UCPMap* map, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
    return Range(ucpmap_getRange, map, start, option, surrogateValue, filter, context, value);
}

UCPTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t type, std::int32_t valueWidth, const void* data, std::int32_t length, std::int32_t* actualLength, std::int32_t* error) {
    UErrorCode* status = Status(error);
    if (U_FAILURE(*status)) return nullptr;
    if (!data && length > 0) APS5_INVALID_ARG_EX;
    return ucptrie_openFromBinary(static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth), data, length, actualLength, status);
}

void APS5_VABI ucptrie_close_nid_postfix(UCPTrie* trie) {
    ucptrie_close(trie);
}

std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const UCPTrie* trie) {
    return ucptrie_getType(Required(trie));
}

std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const UCPTrie* trie) {
    return ucptrie_getValueWidth(Required(trie));
}

std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const UCPTrie* trie, std::int32_t c) {
    return ucptrie_get(Required(trie), c);
}

std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const UCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
    return Range(ucptrie_getRange, trie, start, option, surrogateValue, filter, context, value);
}

std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const UCPTrie* trie, void* data, std::int32_t capacity, std::int32_t* error) {
    UErrorCode* status = Status(error);
    if (U_FAILURE(*status)) return 0;
    return ucptrie_toBinary(Required(trie), data, capacity, status);
}

std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const UCPTrie* trie, std::int32_t c) {
    return ucptrie_internalSmallIndex(Required(trie), c);
}

std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const UCPTrie* trie, std::int32_t lt1, std::uint8_t t2, std::uint8_t t3) {
    return ucptrie_internalSmallU8Index(Required(trie), lt1, t2, t3);
}

std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const UCPTrie* trie, std::int32_t c, const std::uint8_t* start, const std::uint8_t* src) {
    return ucptrie_internalU8PrevIndex(Required(trie), c, Required(start), Required(src));
}

UMutableCPTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t initialValue, std::uint32_t errorValue, std::int32_t* error) {
    return umutablecptrie_open(initialValue, errorValue, Status(error));
}

UMutableCPTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const UMutableCPTrie* other, std::int32_t* error) {
    return umutablecptrie_clone(other, Status(error));
}

void APS5_VABI umutablecptrie_close_nid_postfix(UMutableCPTrie* trie) {
    umutablecptrie_close(trie);
}

UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const UCPMap* map, std::int32_t* error) {
    return umutablecptrie_fromUCPMap(map, Status(error));
}

UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const UCPTrie* trie, std::int32_t* error) {
    return umutablecptrie_fromUCPTrie(trie, Status(error));
}

std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const UMutableCPTrie* trie, std::int32_t c) {
    return umutablecptrie_get(Required(trie), c);
}

std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const UMutableCPTrie* trie, std::int32_t start, std::int32_t option, std::uint32_t surrogateValue, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
    return Range(umutablecptrie_getRange, trie, start, option, surrogateValue, filter, context, value);
}

void APS5_VABI umutablecptrie_set_nid_postfix(UMutableCPTrie* trie, std::int32_t c, std::uint32_t value, std::int32_t* error) {
    UErrorCode* status = Status(error);
    if (U_FAILURE(*status)) return;
    umutablecptrie_set(Required(trie), c, value, status);
}

void APS5_VABI umutablecptrie_setRange_nid_postfix(UMutableCPTrie* trie, std::int32_t start, std::int32_t end, std::uint32_t value, std::int32_t* error) {
    UErrorCode* status = Status(error);
    if (U_FAILURE(*status)) return;
    umutablecptrie_setRange(Required(trie), start, end, value, status);
}

UCPTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(UMutableCPTrie* trie, std::int32_t type, std::int32_t valueWidth, std::int32_t* error) {
    UErrorCode* status = Status(error);
    if (U_FAILURE(*status)) return nullptr;
    return umutablecptrie_buildImmutable(Required(trie), static_cast<UCPTrieType>(type), static_cast<UCPTrieValueWidth>(valueWidth), status);
}

}
