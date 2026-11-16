// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "UCharsTrie.hpp"

namespace Icu::Abi {

struct CharacterIterator;
struct ForwardCharacterIterator;
struct CharacterIteratorFunctions {
    void (APS5_VABI *destroy)(ForwardCharacterIterator*);
    void (APS5_VABI *deleteObject)(ForwardCharacterIterator*);
    void* (APS5_VABI *getDynamicClassId)(const ForwardCharacterIterator*);
    std::int8_t (APS5_VABI *equals)(const ForwardCharacterIterator*, const ForwardCharacterIterator*);
    std::int32_t (APS5_VABI *hash)(const ForwardCharacterIterator*);
    char16_t (APS5_VABI *nextPostInc)(ForwardCharacterIterator*);
    std::int32_t (APS5_VABI *next32PostInc)(ForwardCharacterIterator*);
    std::int8_t (APS5_VABI *hasNext)(ForwardCharacterIterator*);
    CharacterIterator* (APS5_VABI *clone)(const CharacterIterator*);
    char16_t (APS5_VABI *first)(CharacterIterator*);
    char16_t (APS5_VABI *firstPostInc)(CharacterIterator*);
    std::int32_t (APS5_VABI *first32)(CharacterIterator*);
    std::int32_t (APS5_VABI *first32PostInc)(CharacterIterator*);
    char16_t (APS5_VABI *last)(CharacterIterator*);
    std::int32_t (APS5_VABI *last32)(CharacterIterator*);
    char16_t (APS5_VABI *setIndex)(CharacterIterator*, std::int32_t);
    std::int32_t (APS5_VABI *setIndex32)(CharacterIterator*, std::int32_t);
    char16_t (APS5_VABI *current)(const CharacterIterator*);
    std::int32_t (APS5_VABI *current32)(const CharacterIterator*);
    char16_t (APS5_VABI *next)(CharacterIterator*);
    std::int32_t (APS5_VABI *next32)(CharacterIterator*);
    char16_t (APS5_VABI *previous)(CharacterIterator*);
    std::int32_t (APS5_VABI *previous32)(CharacterIterator*);
    std::int8_t (APS5_VABI *hasPrevious)(CharacterIterator*);
    std::int32_t (APS5_VABI *move)(CharacterIterator*, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *move32)(CharacterIterator*, std::int32_t, std::int32_t);
    void (APS5_VABI *getText)(CharacterIterator*, void*);
};

struct ForwardCharacterIterator { const CharacterIteratorFunctions* vtable; };
struct CharacterIterator {
    const CharacterIteratorFunctions* vtable;
    std::int32_t textLength;
    std::int32_t position;
    std::int32_t begin;
    std::int32_t end;
};

struct UCharCharacterIterator {
    CharacterIterator base;
    const char16_t* text;
};

static_assert(sizeof(ForwardCharacterIterator) == 8 && alignof(ForwardCharacterIterator) == 8);
static_assert(offsetof(ForwardCharacterIterator, vtable) == 0);
static_assert(sizeof(CharacterIterator) == 24 && alignof(CharacterIterator) == 8);
static_assert(offsetof(CharacterIterator, textLength) == 8 && offsetof(CharacterIterator, position) == 12);
static_assert(offsetof(CharacterIterator, begin) == 16 && offsetof(CharacterIterator, end) == 20);
static_assert(sizeof(UCharCharacterIterator) == 32 && alignof(UCharCharacterIterator) == 8);
static_assert(offsetof(UCharCharacterIterator, base) == 0 && offsetof(UCharCharacterIterator, text) == 24);
static_assert(sizeof(CharacterIteratorFunctions) == 216 && alignof(CharacterIteratorFunctions) == 8);
static_assert(offsetof(CharacterIteratorFunctions, getDynamicClassId) == 16);
static_assert(offsetof(CharacterIteratorFunctions, nextPostInc) == 40 && offsetof(CharacterIteratorFunctions, clone) == 64);
static_assert(offsetof(CharacterIteratorFunctions, move) == 192 && offsetof(CharacterIteratorFunctions, getText) == 208);

}
extern "C" {
Icu::Abi::CharacterIterator* APS5_VABI _ZN6icu_6817CharacterIteratoraSERKS0__nid_postfix(Icu::Abi::CharacterIterator* self, const Icu::Abi::CharacterIterator* source);
char16_t APS5_VABI _ZN6icu_6817CharacterIterator12firstPostIncEv_nid_postfix(Icu::Abi::CharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6817CharacterIterator14first32PostIncEv_nid_postfix(Icu::Abi::CharacterIterator* self);
Icu::Abi::UCharCharacterIterator* APS5_VABI _ZN6icu_6822UCharCharacterIteratoraSERKS0__nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::UCharCharacterIterator* source);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator12firstPostIncEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4lastEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix(Icu::Abi::UCharCharacterIterator* self, std::int32_t position);
char16_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator7currentEv_nid_postfix(const Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
char16_t APS5_VABI _ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int8_t APS5_VABI _ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int8_t APS5_VABI _ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator14first32PostIncEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6last32Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator10setIndex32Ei_nid_postfix(Icu::Abi::UCharCharacterIterator* self, std::int32_t position);
std::int32_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator9current32Ev_nid_postfix(const Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator10previous32Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix(Icu::Abi::UCharCharacterIterator* self, std::int32_t delta, std::int32_t origin);
std::int32_t APS5_VABI _ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix(Icu::Abi::UCharCharacterIterator* self, std::int32_t delta, std::int32_t origin);
void APS5_VABI _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length);
std::int32_t APS5_VABI _ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix(const Icu::Abi::UCharCharacterIterator* self);
void* APS5_VABI _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6822UCharCharacterIterator17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UCharCharacterIterator*);
}
