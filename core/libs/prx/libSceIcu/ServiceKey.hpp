// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"

namespace Icu::Abi {
struct ICUServiceKey { const GuestFunction* vtable; UnicodeString id; };
struct ServiceKeyFunctions {
    void (APS5_VABI *destroy)(ICUServiceKey*);
    void (APS5_VABI *deleteObject)(ICUServiceKey*);
    void* (APS5_VABI *getDynamicClassId)(const ICUServiceKey*);
    const UnicodeString* (APS5_VABI *getId)(const ICUServiceKey*);
    UnicodeString* (APS5_VABI *canonicalId)(const ICUServiceKey*, UnicodeString*);
    UnicodeString* (APS5_VABI *currentId)(const ICUServiceKey*, UnicodeString*);
    UnicodeString* (APS5_VABI *currentDescriptor)(const ICUServiceKey*, UnicodeString*);
    std::int8_t (APS5_VABI *fallback)(ICUServiceKey*);
    std::int8_t (APS5_VABI *isFallbackOf)(const ICUServiceKey*, const UnicodeString*);
    UnicodeString* (APS5_VABI *prefix)(const ICUServiceKey*, UnicodeString*);
};
static_assert(sizeof(ICUServiceKey) == 72 && alignof(ICUServiceKey) == 8);
static_assert(offsetof(ICUServiceKey, vtable) == 0 && offsetof(ICUServiceKey, id) == 8);
static_assert(sizeof(ServiceKeyFunctions) == 80 && alignof(ServiceKeyFunctions) == 8);
static_assert(offsetof(ServiceKeyFunctions, getId) == 24 && offsetof(ServiceKeyFunctions, prefix) == 72);
}

extern "C" {
void APS5_VABI _ZN6icu_6813ICUServiceKeyC1ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::ICUServiceKey* self, const Icu::Abi::UnicodeString* id);
void APS5_VABI _ZN6icu_6813ICUServiceKeyC2ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::ICUServiceKey* self, const Icu::Abi::UnicodeString* id);
void APS5_VABI _ZN6icu_6813ICUServiceKeyD0Ev_nid_postfix(Icu::Abi::ICUServiceKey* self);
void APS5_VABI _ZN6icu_6813ICUServiceKeyD1Ev_nid_postfix(Icu::Abi::ICUServiceKey* self);
void APS5_VABI _ZN6icu_6813ICUServiceKeyD2Ev_nid_postfix(Icu::Abi::ICUServiceKey* self);
void* APS5_VABI _ZN6icu_6813ICUServiceKey16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6813ICUServiceKey17getDynamicClassIDEv_nid_postfix(const Icu::Abi::ICUServiceKey* self);
const Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey5getIDEv_nid_postfix(const Icu::Abi::ICUServiceKey* self);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey11canonicalIDERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUServiceKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey9currentIDERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUServiceKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUServiceKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey6prefixERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUServiceKey* self, Icu::Abi::UnicodeString* result);
std::int8_t APS5_VABI _ZN6icu_6813ICUServiceKey8fallbackEv_nid_postfix(Icu::Abi::ICUServiceKey* self);
std::int8_t APS5_VABI _ZNK6icu_6813ICUServiceKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUServiceKey* self, const Icu::Abi::UnicodeString* id);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813ICUServiceKey11parsePrefixERNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813ICUServiceKey11parseSuffixERNS_13UnicodeStringE_nid_postfix(Icu::Abi::UnicodeString* result);
}
