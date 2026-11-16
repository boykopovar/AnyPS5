// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2002-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2014, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include "StringsEnumeration.hpp"

namespace Icu::Abi {
struct StringEnumeration {
    const GuestFunction* vtable;
    UnicodeString unistr;
    char charsBuffer[32];
    char* chars;
    std::int32_t charsCapacity;
};
struct UStringEnumeration {
    StringEnumeration base;
    Icu::UEnumeration* enumeration;
};
static_assert(sizeof(StringEnumeration) == 120 && alignof(StringEnumeration) == 8);
static_assert(offsetof(StringEnumeration, vtable) == 0 && offsetof(StringEnumeration, unistr) == 8);
static_assert(offsetof(StringEnumeration, charsBuffer) == 72 && offsetof(StringEnumeration, chars) == 104);
static_assert(offsetof(StringEnumeration, charsCapacity) == 112);
static_assert(sizeof(UStringEnumeration) == 128 && alignof(UStringEnumeration) == 8);
static_assert(offsetof(UStringEnumeration, base) == 0 && offsetof(UStringEnumeration, enumeration) == 120);
}

extern "C" {
void APS5_VABI _ZN6icu_6817StringEnumerationC2Ev_nid_postfix(Icu::Abi::StringEnumeration* self);
void APS5_VABI _ZN6icu_6817StringEnumerationD1Ev_nid_postfix(Icu::Abi::StringEnumeration* self);
void APS5_VABI _ZN6icu_6817StringEnumerationD2Ev_nid_postfix(Icu::Abi::StringEnumeration* self);
void APS5_VABI _ZN6icu_6817StringEnumerationD0Ev_nid_postfix(Icu::Abi::StringEnumeration* self);
void APS5_VABI _ZN6icu_6818UStringEnumerationD1Ev_nid_postfix(Icu::Abi::UStringEnumeration* self);
void APS5_VABI _ZN6icu_6818UStringEnumerationD2Ev_nid_postfix(Icu::Abi::UStringEnumeration* self);
void APS5_VABI _ZN6icu_6818UStringEnumerationD0Ev_nid_postfix(Icu::Abi::UStringEnumeration* self);
Icu::Abi::StringEnumeration* APS5_VABI _ZNK6icu_6817StringEnumeration5cloneEv_nid_postfix(const Icu::Abi::StringEnumeration*);
void APS5_VABI _ZN6icu_6817StringEnumeration19ensureCharsCapacityEiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t capacity, std::int32_t* status);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817StringEnumeration8setCharsEPKciR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, const char* text, std::int32_t length, std::int32_t* status);
const Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817StringEnumeration5snextER10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* status);
const char16_t* APS5_VABI _ZN6icu_6817StringEnumeration5unextEPiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* length, std::int32_t* status);
const char* APS5_VABI _ZN6icu_6817StringEnumeration4nextEPiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* length, std::int32_t* status);
std::int8_t APS5_VABI _ZNK6icu_6817StringEnumerationeqERKS0__nid_postfix(const Icu::Abi::StringEnumeration* self, const Icu::Abi::StringEnumeration* other);
std::int8_t APS5_VABI _ZNK6icu_6817StringEnumerationneERKS0__nid_postfix(const Icu::Abi::StringEnumeration* self, const Icu::Abi::StringEnumeration* other);
void APS5_VABI _ZN6icu_6818UStringEnumerationC1EP12UEnumeration_nid_postfix(Icu::Abi::UStringEnumeration* self, Icu::UEnumeration* enumeration);
void APS5_VABI _ZN6icu_6818UStringEnumerationC2EP12UEnumeration_nid_postfix(Icu::Abi::UStringEnumeration* self, Icu::UEnumeration* enumeration);
Icu::Abi::UStringEnumeration* APS5_VABI _ZN6icu_6818UStringEnumeration16fromUEnumerationEP12UEnumerationR10UErrorCode_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* status);
std::int32_t APS5_VABI _ZNK6icu_6818UStringEnumeration5countER10UErrorCode_nid_postfix(const Icu::Abi::UStringEnumeration* self, std::int32_t* status);
const char* APS5_VABI _ZN6icu_6818UStringEnumeration4nextEPiR10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* length, std::int32_t* status);
const Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6818UStringEnumeration5snextER10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* status);
void APS5_VABI _ZN6icu_6818UStringEnumeration5resetER10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* status);
void* APS5_VABI _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UStringEnumeration*);
Icu::UEnumeration* APS5_VABI uenum_openFromStringEnumeration_nid_postfix(Icu::Abi::StringEnumeration* adopted, std::int32_t* status);
}
