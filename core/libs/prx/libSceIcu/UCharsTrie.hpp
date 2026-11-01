// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Vector.hpp"
#include "Appendable.hpp"

namespace Icu::Abi {

struct UCharsTrie {
    char16_t* ownedArray;
    const char16_t* units;
    const char16_t* position;
    std::int32_t remaining;
};

struct AppendableFunctions {
    void (APS5_VABI *destroy)(Appendable*);
    void (APS5_VABI *deleteObject)(Appendable*);
    void* (APS5_VABI *getDynamicClassId)(const Appendable*);
    std::int8_t (APS5_VABI *appendUnit)(Appendable*, char16_t);
    std::int8_t (APS5_VABI *appendPoint)(Appendable*, std::int32_t);
    std::int8_t (APS5_VABI *appendString)(Appendable*, const char16_t*, std::int32_t);
    std::int8_t (APS5_VABI *reserve)(Appendable*, std::int32_t);
    char16_t* (APS5_VABI *getAppendBuffer)(Appendable*, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
};

static_assert(sizeof(UCharsTrie) == 32 && alignof(UCharsTrie) == 8);
static_assert(offsetof(UCharsTrie, ownedArray) == 0 && offsetof(UCharsTrie, units) == 8);
static_assert(offsetof(UCharsTrie, position) == 16 && offsetof(UCharsTrie, remaining) == 24);
static_assert(sizeof(AppendableFunctions) == 64 && alignof(AppendableFunctions) == 8);
static_assert(offsetof(AppendableFunctions, appendUnit) == 24 && offsetof(AppendableFunctions, reserve) == 48);

}
extern "C" {
void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(Icu::Abi::UCharsTrie* self);
void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(Icu::Abi::UCharsTrie* self);
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const Icu::Abi::UCharsTrie* self);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(Icu::Abi::UCharsTrie* self, const char16_t* position, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(Icu::Abi::UCharsTrie* self, const char16_t* position, std::int32_t length, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(Icu::Abi::UCharsTrie* self, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(Icu::Abi::UCharsTrie* self, std::int32_t point);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(Icu::Abi::UCharsTrie* self, std::int32_t point);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UCharsTrie* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length);
std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t* position, std::int8_t haveValue, std::int32_t* value);
const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t* position, std::int32_t length, std::int8_t haveValue, std::int32_t* value);
void APS5_VABI _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(const char16_t* position, std::int32_t length, Icu::Abi::Appendable* output);
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(const Icu::Abi::UCharsTrie* self, Icu::Abi::Appendable* output);
}
