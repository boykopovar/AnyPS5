#include "prx/libSceIcu/CharacterNames.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "uprops.h"
#include "uset_imp.h"
#include <cstdint>

namespace {

struct NameEnumeration {
    Icu::CharacterNameVisitor visitor;
    void* context;
};

UBool U_CALLCONV ForwardName(void* context, UChar32 character, UCharNameChoice choice, const char* name, int32_t length) {
    const auto& enumeration = *static_cast<const NameEnumeration*>(context);
    return enumeration.visitor(enumeration.context, character, choice, name, length) != 0;
}

void U_CALLCONV ForwardAdd(USet* set, UChar32 character) {
    const auto& adder = *reinterpret_cast<const Icu::SetAdder*>(set);
    adder.add(adder.set, character);
}

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

UCharNameChoice Choice(std::int32_t choice) {
    return static_cast<UCharNameChoice>(choice < 0 ? U_CHAR_NAME_CHOICE_COUNT : choice);
}

}

extern "C" {

std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t character, std::int32_t choice, char* buffer, std::int32_t capacity,
                                              std::int32_t* error) {
    return u_charName(character, Choice(choice), buffer, capacity, &Status(error));
}

std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t choice, const char* name, std::int32_t* error) {
    return u_charFromName(Choice(choice), name, &Status(error));
}

void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t start, std::int32_t limit, Icu::CharacterNameVisitor visitor, void* context,
                                           std::int32_t choice, std::int32_t* error) {
    NameEnumeration enumeration{visitor, context};
    u_enumCharNames(start, limit, visitor ? ForwardName : nullptr, &enumeration, Choice(choice), &Status(error));
}

std::int32_t APS5_VABI uprv_getMaxCharNameLength_nid_postfix() {
    return uprv_getMaxCharNameLength();
}

void APS5_VABI uprv_getCharNameCharacters_nid_postfix(const Icu::SetAdder* adder) {
    if (!adder || !adder->add) APS5_INVALID_ARG_EX;
    USetAdder forward{reinterpret_cast<USet*>(const_cast<Icu::SetAdder*>(adder)), ForwardAdd, nullptr, nullptr, nullptr, nullptr};
    uprv_getCharNameCharacters(&forward);
}

}
