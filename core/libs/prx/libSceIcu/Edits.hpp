// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Vector.hpp"

namespace Icu::Abi {

struct Edits {
    std::uint16_t* array;
    std::int32_t capacity;
    std::int32_t length;
    std::int32_t delta;
    std::int32_t changes;
    std::int32_t error;
    std::uint16_t stack[100];
};

struct EditsIterator {
    const std::uint16_t* array;
    std::int32_t index;
    std::int32_t length;
    std::int32_t remaining;
    std::int8_t onlyChanges;
    std::int8_t coarse;
    std::int8_t direction;
    std::int8_t changed;
    std::int32_t oldLength;
    std::int32_t newLength;
    std::int32_t sourceIndex;
    std::int32_t replacementIndex;
    std::int32_t destinationIndex;
};

static_assert(sizeof(Edits) == 232 && alignof(Edits) == 8);
static_assert(offsetof(Edits, capacity) == 8 && offsetof(Edits, length) == 12);
static_assert(offsetof(Edits, delta) == 16 && offsetof(Edits, changes) == 20);
static_assert(offsetof(Edits, error) == 24 && offsetof(Edits, stack) == 28);
static_assert(sizeof(EditsIterator) == 48 && alignof(EditsIterator) == 8);
static_assert(offsetof(EditsIterator, index) == 8 && offsetof(EditsIterator, remaining) == 16);
static_assert(offsetof(EditsIterator, onlyChanges) == 20 && offsetof(EditsIterator, direction) == 22);
static_assert(offsetof(EditsIterator, changed) == 23 && offsetof(EditsIterator, oldLength) == 24);
static_assert(offsetof(EditsIterator, sourceIndex) == 32 && offsetof(EditsIterator, destinationIndex) == 40);

}

extern "C" {
void APS5_VABI _ZN6icu_685Edits12releaseArrayEv_nid_postfix(Icu::Abi::Edits* self);
void APS5_VABI _ZN6icu_685EditsD1Ev_nid_postfix(Icu::Abi::Edits* self);
void APS5_VABI _ZN6icu_685EditsD2Ev_nid_postfix(Icu::Abi::Edits* self);
void APS5_VABI _ZN6icu_685Edits5resetEv_nid_postfix(Icu::Abi::Edits* self);
void APS5_VABI _ZN6icu_685Edits6appendEi_nid_postfix(Icu::Abi::Edits* self, std::int32_t unit);
std::int8_t APS5_VABI _ZN6icu_685Edits9growArrayEv_nid_postfix(Icu::Abi::Edits* self);
void APS5_VABI _ZN6icu_685Edits12addUnchangedEi_nid_postfix(Icu::Abi::Edits* self, std::int32_t length);
void APS5_VABI _ZN6icu_685Edits10addReplaceEii_nid_postfix(Icu::Abi::Edits* self, std::int32_t oldLength, std::int32_t newLength);
Icu::Abi::Edits* APS5_VABI _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(Icu::Abi::Edits* self, const Icu::Abi::Edits* source);
Icu::Abi::Edits* APS5_VABI _ZN6icu_685Edits9moveArrayERS0__nid_postfix(Icu::Abi::Edits* self, Icu::Abi::Edits* source);
Icu::Abi::Edits* APS5_VABI _ZN6icu_685EditsaSERKS0__nid_postfix(Icu::Abi::Edits* self, const Icu::Abi::Edits* source);
Icu::Abi::Edits* APS5_VABI _ZN6icu_685EditsaSEOS0__nid_postfix(Icu::Abi::Edits* self, Icu::Abi::Edits* source);
std::int8_t APS5_VABI _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(const Icu::Abi::Edits* self, std::int32_t* status);
void APS5_VABI _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(Icu::Abi::EditsIterator* self, const std::uint16_t* array, std::int32_t length, std::int8_t onlyChanges, std::int8_t coarse);
void APS5_VABI _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(Icu::Abi::EditsIterator* self, const std::uint16_t* array, std::int32_t length, std::int8_t onlyChanges, std::int8_t coarse);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(Icu::Abi::EditsIterator* self, std::int32_t head);
void APS5_VABI _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(Icu::Abi::EditsIterator* self);
void APS5_VABI _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(Icu::Abi::EditsIterator* self);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(Icu::Abi::EditsIterator* self);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(Icu::Abi::EditsIterator* self, std::int8_t onlyChanges, std::int32_t* status);
std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(Icu::Abi::EditsIterator* self, std::int32_t* status);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(Icu::Abi::EditsIterator* self, std::int32_t index, std::int8_t source, std::int32_t* status);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(Icu::Abi::EditsIterator* self, std::int32_t index, std::int32_t* status);
std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(Icu::Abi::EditsIterator* self, std::int32_t index, std::int32_t* status);
Icu::Abi::Edits* APS5_VABI _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(Icu::Abi::Edits* self, const Icu::Abi::Edits* first, const Icu::Abi::Edits* second, std::int32_t* status);
}
