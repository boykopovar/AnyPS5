// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2003, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2006, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include "Vector.hpp"

namespace Icu::Abi {

struct UnicodeSet {
    const GuestFunction* vtable;
    const GuestFunction* matcherVtable;
    std::int32_t* list;
    std::int32_t capacity;
    std::int32_t len;
    std::uint8_t fFlags;
    void* bmpSet;
    std::int32_t* buffer;
    std::int32_t bufferCapacity;
    char16_t* pat;
    std::int32_t patLen;
    Vector* strings;
    void* stringSpan;
    std::int32_t stackList[25];
};

static_assert(sizeof(UnicodeSet) == 200 && alignof(UnicodeSet) == 8);
static_assert(offsetof(UnicodeSet, matcherVtable) == 8 && offsetof(UnicodeSet, list) == 16);
static_assert(offsetof(UnicodeSet, capacity) == 24 && offsetof(UnicodeSet, len) == 28);
static_assert(offsetof(UnicodeSet, fFlags) == 32 && offsetof(UnicodeSet, bmpSet) == 40);
static_assert(offsetof(UnicodeSet, buffer) == 48 && offsetof(UnicodeSet, bufferCapacity) == 56);
static_assert(offsetof(UnicodeSet, pat) == 64 && offsetof(UnicodeSet, patLen) == 72);
static_assert(offsetof(UnicodeSet, strings) == 80 && offsetof(UnicodeSet, stringSpan) == 88);
static_assert(offsetof(UnicodeSet, stackList) == 96);

}

extern "C" {
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6addAllERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet9retainAllERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet9removeAllERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet13complementAllERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet10createFromERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet13createFromAllERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet9retainAllERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet9removeAllERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet13complementAllERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
void APS5_VABI _ZNK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix(const Icu::Abi::UnicodeSet* self, Icu::Abi::UnicodeSet* other);
void APS5_VABI _ZThn8_NK6icu_6810UnicodeSet13addMatchSetToERS0__nid_postfix(const void* self, Icu::Abi::UnicodeSet* other);
std::int8_t APS5_VABI _ZThn8_NK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix(const void* self, std::uint8_t value);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet9serializeEPtiR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeSet* self, std::uint16_t* destination, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_6810UnicodeSetC1Ev_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSetC1Eii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
void APS5_VABI _ZN6icu_6810UnicodeSetC2Ev_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSetC2Eii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
void APS5_VABI _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSetD2Ev_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSetD0Ev_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZThn8_N6icu_6810UnicodeSetD1Ev_nid_postfix(void* self);
void APS5_VABI _ZThn8_N6icu_6810UnicodeSetD0Ev_nid_postfix(void* self);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet3addEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t point);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet3addEii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6retainEii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6retainEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t point);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6removeEii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6removeEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t point);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet10complementEii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet10complementEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t point);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet3setEii_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet10complementEv_nid_postfix(Icu::Abi::UnicodeSet* self);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet5clearEv_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSet10setToBogusEv_nid_postfix(Icu::Abi::UnicodeSet* self);
std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet12nextCapacityEi_nid_postfix(std::int32_t minimum);
bool APS5_VABI _ZN6icu_6810UnicodeSet14ensureCapacityEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t length);
bool APS5_VABI _ZN6icu_6810UnicodeSet20ensureBufferCapacityEi_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t length);
void APS5_VABI _ZN6icu_6810UnicodeSet11swapBuffersEv_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSet14releasePatternEv_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSet10setPatternEPKDsi_nid_postfix(Icu::Abi::UnicodeSet* self, const char16_t* pattern, std::int32_t length);
void APS5_VABI _ZN6icu_6810UnicodeSet3addEPKiia_nid_postfix(Icu::Abi::UnicodeSet* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity);
void APS5_VABI _ZN6icu_6810UnicodeSet6retainEPKiia_nid_postfix(Icu::Abi::UnicodeSet* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity);
void APS5_VABI _ZN6icu_6810UnicodeSet11exclusiveOrEPKiia_nid_postfix(Icu::Abi::UnicodeSet* self, const std::int32_t* list, std::int32_t length, std::int8_t polarity);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13findCodePointEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t point);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t index);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t index);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t point);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsEii_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet12containsNoneEii_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t start, std::int32_t end);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet8hashCodeEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet7isEmptyEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet10hasStringsEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet11stringsSizeEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
const Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6810UnicodeSet9getStringEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t index);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet15stringsContainsERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet11getSingleCPERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::UnicodeString* text);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet8containsERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet3addERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6removeERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet10complementERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
void APS5_VABI _ZN6icu_6810UnicodeSet4_addERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
std::int8_t APS5_VABI _ZN6icu_6810UnicodeSet15allocateStringsER10UErrorCode_nid_postfix(Icu::Abi::UnicodeSet* self, std::int32_t* status);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet6addAllERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeString* text);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet11containsAllERKS0__nid_postfix(const Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet12containsNoneERKS0__nid_postfix(const Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSeteqERKS0__nid_postfix(const Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet6charAtEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t index);
std::int32_t APS5_VABI _ZNK6icu_6810UnicodeSet7indexOfEi_nid_postfix(const Icu::Abi::UnicodeSet* self, std::int32_t point);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet16removeAllStringsEv_nid_postfix(Icu::Abi::UnicodeSet* self);
void* APS5_VABI _ZNK6icu_6810UnicodeSet17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UnicodeSet*);
void* APS5_VABI _ZN6icu_6810UnicodeSet16getStaticClassIDEv_nid_postfix();
std::int8_t APS5_VABI _ZNK6icu_6810UnicodeSet17matchesIndexValueEh_nid_postfix(const Icu::Abi::UnicodeSet* self, std::uint8_t value);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet7compactEv_nid_postfix(Icu::Abi::UnicodeSet* self);
void APS5_VABI _ZN6icu_6810UnicodeSetC1ERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
void APS5_VABI _ZN6icu_6810UnicodeSetC2ERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
void APS5_VABI _ZN6icu_6810UnicodeSetC1ERKS0_a_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other, std::int8_t asThawed);
void APS5_VABI _ZN6icu_6810UnicodeSetC2ERKS0_a_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other, std::int8_t asThawed);
void APS5_VABI _ZN6icu_6810UnicodeSetC1EPKtiNS0_14ESerializationER10UErrorCode_nid_postfix(Icu::Abi::UnicodeSet* self, const std::uint16_t* data, std::int32_t dataLen, std::int32_t serialization, std::int32_t* status);
void APS5_VABI _ZN6icu_6810UnicodeSetC2EPKtiNS0_14ESerializationER10UErrorCode_nid_postfix(Icu::Abi::UnicodeSet* self, const std::uint16_t* data, std::int32_t dataLen, std::int32_t serialization, std::int32_t* status);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSetaSERKS0__nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other);
Icu::Abi::UnicodeSet* APS5_VABI _ZN6icu_6810UnicodeSet8copyFromERKS0_a_nid_postfix(Icu::Abi::UnicodeSet* self, const Icu::Abi::UnicodeSet* other, std::int8_t asThawed);
Icu::Abi::UnicodeSet* APS5_VABI _ZNK6icu_6810UnicodeSet5cloneEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
Icu::Abi::UnicodeSet* APS5_VABI _ZNK6icu_6810UnicodeSet13cloneAsThawedEv_nid_postfix(const Icu::Abi::UnicodeSet* self);
std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix(Icu::Abi::UnicodeSet* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental);
std::int32_t APS5_VABI _ZThn8_N6icu_6810UnicodeSet7matchesERKNS_11ReplaceableERiia_nid_postfix(void* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental);
std::int32_t APS5_VABI _ZN6icu_6810UnicodeSet9matchRestERKNS_11ReplaceableEiiRKNS_13UnicodeStringE_nid_postfix(const void* text, std::int32_t start, std::int32_t limit, const Icu::Abi::UnicodeString* s);
void APS5_VABI _ZN6icu_6810UnicodeSet11applyFilterEPFaiPvES1_PKS0_R10UErrorCode_nid_postfix(Icu::Abi::UnicodeSet* self, std::int8_t (APS5_VABI *filter)(std::int32_t, void*), void* context, const Icu::Abi::UnicodeSet* inclusions, std::int32_t* status);
void APS5_VABI _ZN6icu_6810UStringSetD0Ev_nid_postfix(Icu::Abi::Vector* self);
void APS5_VABI _ZN6icu_6810UStringSetD1Ev_nid_postfix(Icu::Abi::Vector* self);
void APS5_VABI _ZN6icu_6810UStringSetD2Ev_nid_postfix(Icu::Abi::Vector* self);
}
