#include "prx/libSceIcu/LegacyTrie.hpp"
#include "prx/libc/include/General.hpp"
#include "utrie2.h"
#include <cstdint>

namespace {

struct Enumeration {
    Icu::Trie2EnumValue value;
    Icu::Trie2EnumRange range;
    const void* context;
};

std::uint32_t U_CALLCONV ForwardValue(const void* context, std::uint32_t value) {
    const auto& enumeration = *static_cast<const Enumeration*>(context);
    return enumeration.value(enumeration.context, value);
}

UBool U_CALLCONV ForwardRange(const void* context, UChar32 start, UChar32 end, std::uint32_t value) {
    const auto& enumeration = *static_cast<const Enumeration*>(context);
    return enumeration.range(enumeration.context, start, end, value) != 0;
}

const UTrie2* Native(const Icu::Trie2* trie) {
    return reinterpret_cast<const UTrie2*>(trie);
}

Icu::Trie2* Guest(UTrie2* trie) {
    return reinterpret_cast<Icu::Trie2*>(trie);
}

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(error);
}

template<typename TEnumerate>
void Enumerate(const Icu::Trie2* trie, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange, const void* context,
               const TEnumerate& enumerate) {
    if (!enumRange) return;
    if (!trie) APS5_INVALID_ARG_EX;
    const Enumeration forward{enumValue, enumRange, context};
    enumerate(enumValue ? ForwardValue : nullptr, ForwardRange, &forward);
}

}

extern "C" {

Icu::Trie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(std::int32_t valueBits, const void* data, std::int32_t length,
                                                             std::int32_t* actualLength, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error <= 0 && length > 0 && !data) APS5_INVALID_ARG_EX;
    return Guest(utrie2_openFromSerialized(static_cast<UTrie2ValueBits>(valueBits), data, length, actualLength, Status(error)));
}

Icu::Trie2* APS5_VABI utrie2_openDummy_nid_postfix(std::int32_t valueBits, std::uint32_t initialValue, std::uint32_t errorValue,
                                                    std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return Guest(utrie2_openDummy(static_cast<UTrie2ValueBits>(valueBits), initialValue, errorValue, Status(error)));
}

void APS5_VABI utrie2_close_nid_postfix(Icu::Trie2* trie) {
    utrie2_close(reinterpret_cast<UTrie2*>(trie));
}

std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const Icu::Trie2* trie) {
    if (!trie) APS5_INVALID_ARG_EX;
    return utrie2_isFrozen(Native(trie));
}

std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint) {
    if (!trie) APS5_INVALID_ARG_EX;
    return utrie2_get32(Native(trie), codePoint);
}

std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const Icu::Trie2* trie, std::int32_t codeUnit) {
    if (!trie) APS5_INVALID_ARG_EX;
    return utrie2_get32FromLeadSurrogateCodeUnit(Native(trie), codeUnit);
}

std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const Icu::Trie2* trie, void* data, std::int32_t capacity, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return utrie2_serialize(Native(trie), data, capacity, Status(error));
}

void APS5_VABI utrie2_enum_nid_postfix(const Icu::Trie2* trie, Icu::Trie2EnumValue enumValue, Icu::Trie2EnumRange enumRange,
                                       const void* context) {
    Enumerate(trie, enumValue, enumRange, context, [&](UTrie2EnumValue* value, UTrie2EnumRange* range, const void* forward) {
        utrie2_enum(Native(trie), value, range, forward);
    });
}

void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const Icu::Trie2* trie, std::int32_t lead, Icu::Trie2EnumValue enumValue,
                                                       Icu::Trie2EnumRange enumRange, const void* context) {
    if ((lead & 0xfffffc00) != 0xd800) return;
    Enumerate(trie, enumValue, enumRange, context, [&](UTrie2EnumValue* value, UTrie2EnumRange* range, const void* forward) {
        utrie2_enumForLeadSurrogate(Native(trie), lead, value, range, forward);
    });
}

std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint,
                                                              const std::uint8_t* source, const std::uint8_t* limit) {
    if (!trie || !source || !limit) APS5_INVALID_ARG_EX;
    return utrie2_internalU8NextIndex(Native(trie), codePoint, source, limit);
}

std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const Icu::Trie2* trie, std::int32_t codePoint,
                                                              const std::uint8_t* start, const std::uint8_t* source) {
    if (!trie || !start || !source) APS5_INVALID_ARG_EX;
    return utrie2_internalU8PrevIndex(Native(trie), codePoint, start, source);
}

}
