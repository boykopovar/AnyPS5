// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2010-2012,2014, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2010-2016, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include "UnicodeString.hpp"
#include "UCharsTrie.hpp"
#include "Hash.hpp"

namespace Icu::Abi {

struct StringTrieBuilder;
struct UCharsTrieBuilder;

struct StringTrieBuilderFunctions {
    void (APS5_VABI *destroy)(void*);
    void (APS5_VABI *deleteObject)(void*);
    void* (APS5_VABI *getDynamicClassId)(const void*);
    std::int32_t (APS5_VABI *getElementStringLength)(const void*, std::int32_t);
    char16_t (APS5_VABI *getElementUnit)(const void*, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *getElementValue)(const void*, std::int32_t);
    std::int32_t (APS5_VABI *getLimitOfLinearMatch)(const void*, std::int32_t, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *countElementUnits)(const void*, std::int32_t, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *skipElementsBySomeUnits)(const void*, std::int32_t, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *indexOfElementWithNextUnit)(const void*, std::int32_t, std::int32_t, char16_t);
    std::int8_t (APS5_VABI *matchNodesCanHaveValues)(const void*);
    std::int32_t (APS5_VABI *getMaxBranchLinearSubNodeLength)(const void*);
    std::int32_t (APS5_VABI *getMinLinearMatch)(const void*);
    std::int32_t (APS5_VABI *getMaxLinearMatchLength)(const void*);
    void* (APS5_VABI *createLinearMatchNode)(const void*, std::int32_t, std::int32_t, std::int32_t, void*);
    std::int32_t (APS5_VABI *writeUnit)(void*, std::int32_t);
    std::int32_t (APS5_VABI *writeElementUnits)(void*, std::int32_t, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *writeValueAndFinal)(void*, std::int32_t, std::int8_t);
    std::int32_t (APS5_VABI *writeValueAndType)(void*, std::int8_t, std::int32_t, std::int32_t);
    std::int32_t (APS5_VABI *writeDeltaTo)(void*, std::int32_t);
};

static_assert(sizeof(StringTrieBuilderFunctions) == 160 && alignof(StringTrieBuilderFunctions) == 8);

struct StringTrieBuilder {
    const StringTrieBuilderFunctions* vtable;
    UHashtable* nodes;
};

static_assert(sizeof(StringTrieBuilder) == 16 && alignof(StringTrieBuilder) == 8);
static_assert(offsetof(StringTrieBuilder, vtable) == 0 && offsetof(StringTrieBuilder, nodes) == 8);

struct UCharsTrieElement {
    std::int32_t stringOffset;
    std::int32_t value;
};

static_assert(sizeof(UCharsTrieElement) == 8 && alignof(UCharsTrieElement) == 4);

struct UCharsTrieBuilder {
    const StringTrieBuilderFunctions* vtable;
    UHashtable* nodes;
    UnicodeString strings;
    UCharsTrieElement* elements;
    std::int32_t elementsCapacity;
    std::int32_t elementsLength;
    char16_t* uchars;
    std::int32_t ucharsCapacity;
    std::int32_t ucharsLength;
};

static_assert(sizeof(UCharsTrieBuilder) == 112 && alignof(UCharsTrieBuilder) == 8);
static_assert(offsetof(UCharsTrieBuilder, strings) == 16 && offsetof(UCharsTrieBuilder, elements) == 80);
static_assert(offsetof(UCharsTrieBuilder, elementsCapacity) == 88 && offsetof(UCharsTrieBuilder, elementsLength) == 92);
static_assert(offsetof(UCharsTrieBuilder, uchars) == 96 && offsetof(UCharsTrieBuilder, ucharsCapacity) == 104);
static_assert(offsetof(UCharsTrieBuilder, ucharsLength) == 108);

}

extern "C" {
void APS5_VABI _ZN6icu_6817StringTrieBuilderC2Ev_nid_postfix(Icu::Abi::StringTrieBuilder* self);
void APS5_VABI _ZN6icu_6817StringTrieBuilderD0Ev_nid_postfix(Icu::Abi::StringTrieBuilder* self);
void APS5_VABI _ZN6icu_6817StringTrieBuilderD1Ev_nid_postfix(Icu::Abi::StringTrieBuilder* self);
void APS5_VABI _ZN6icu_6817StringTrieBuilderD2Ev_nid_postfix(Icu::Abi::StringTrieBuilder* self);
void APS5_VABI _ZN6icu_6817StringTrieBuilder20createCompactBuilderEiR10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t sizeGuess, std::int32_t* errorCode);
void APS5_VABI _ZN6icu_6817StringTrieBuilder20deleteCompactBuilderEv_nid_postfix(Icu::Abi::StringTrieBuilder* self);
void APS5_VABI _ZN6icu_6817StringTrieBuilder5buildE22UStringTrieBuildOptioniR10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t buildOption, std::int32_t elementsLength, std::int32_t* errorCode);
void* APS5_VABI _ZN6icu_6817StringTrieBuilder18registerFinalValueEiR10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t value, std::int32_t* errorCode);
void* APS5_VABI _ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, void* newNode, std::int32_t* errorCode);
void* APS5_VABI _ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t* errorCode);
void* APS5_VABI _ZN6icu_6817StringTrieBuilder17makeBranchSubNodeEiiiiR10UErrorCode_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t length, std::int32_t* errorCode);
std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder18writeBranchSubNodeEiiii_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t length);
std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(Icu::Abi::StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex);
std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder8hashNodeEPKv_nid_postfix(const void* node);
std::int8_t APS5_VABI _ZN6icu_6817StringTrieBuilder10equalNodesEPKvS2__nid_postfix(const void* left, const void* right);

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t* errorCode);
void APS5_VABI _ZN6icu_6817UCharsTrieBuilderC2ER10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t* errorCode);
void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD0Ev_nid_postfix(Icu::Abi::UCharsTrieBuilder* self);
void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(Icu::Abi::UCharsTrieBuilder* self);
void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD2Ev_nid_postfix(Icu::Abi::UCharsTrieBuilder* self);
Icu::Abi::UCharsTrieBuilder* APS5_VABI _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, const Icu::Abi::UnicodeString* s, std::int32_t value, std::int32_t* errorCode);
Icu::Abi::UCharsTrie* APS5_VABI _ZN6icu_6817UCharsTrieBuilder5buildE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t buildOption, std::int32_t* errorCode);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t buildOption, Icu::Abi::UnicodeString* result, std::int32_t* errorCode);
void APS5_VABI _ZN6icu_6817UCharsTrieBuilder11buildUCharsE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t buildOption, std::int32_t* errorCode);
std::int8_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t length);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t unit);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, const char16_t* s, std::int32_t length);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder17writeElementUnitsEiii_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t length);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder18writeValueAndFinalEia_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int8_t isFinal);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder17writeValueAndTypeEaii_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int8_t hasValue, std::int32_t value, std::int32_t node);
std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder12writeDeltaToEi_nid_postfix(Icu::Abi::UCharsTrieBuilder* self, std::int32_t jumpTarget);
std::int8_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23matchNodesCanHaveValuesEv_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23getMaxLinearMatchLengthEv_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder17getMinLinearMatchEv_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder22getElementStringLengthEi_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i);
char16_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder14getElementUnitEii_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder15getElementValueEi_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t first, std::int32_t last, std::int32_t unitIndex);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23skipElementsBySomeUnitsEiii_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder26indexOfElementWithNextUnitEiiDs_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, char16_t unit);
std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder31getMaxBranchLinearSubNodeLengthEv_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self);
void* APS5_VABI _ZNK6icu_6817UCharsTrieBuilder21createLinearMatchNodeEiiiPNS_17StringTrieBuilder4NodeE_nid_postfix(const Icu::Abi::UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t length, void* nextNode);
}
