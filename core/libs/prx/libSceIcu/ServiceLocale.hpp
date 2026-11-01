// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2002-2014, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Service.hpp"

namespace Icu::Abi {
struct LocaleKey {
    const GuestFunction* vtable;
    UnicodeString id;
    std::int32_t kind;
    UnicodeString primaryId;
    UnicodeString fallbackId;
    UnicodeString currentId;
};
struct LocaleKeyFactory { const GuestFunction* vtable; UnicodeString name; std::int32_t coverage; };
struct SimpleLocaleKeyFactory { const GuestFunction* vtable; UnicodeString name; std::int32_t coverage; Object* object; UnicodeString id; std::int32_t kind; };
static_assert(sizeof(LocaleKey) == 272 && alignof(LocaleKey) == 8);
static_assert(offsetof(LocaleKey, vtable) == 0 && offsetof(LocaleKey, id) == 8 && offsetof(LocaleKey, kind) == 72);
static_assert(offsetof(LocaleKey, primaryId) == 80 && offsetof(LocaleKey, fallbackId) == 144 && offsetof(LocaleKey, currentId) == 208);
static_assert(sizeof(LocaleKeyFactory) == 80 && alignof(LocaleKeyFactory) == 8);
static_assert(offsetof(LocaleKeyFactory, vtable) == 0 && offsetof(LocaleKeyFactory, name) == 8 && offsetof(LocaleKeyFactory, coverage) == 72);
static_assert(sizeof(SimpleLocaleKeyFactory) == 160 && alignof(SimpleLocaleKeyFactory) == 8);
static_assert(offsetof(SimpleLocaleKeyFactory, vtable) == 0 && offsetof(SimpleLocaleKeyFactory, name) == 8 && offsetof(SimpleLocaleKeyFactory, coverage) == 72);
static_assert(offsetof(SimpleLocaleKeyFactory, object) == 80 && offsetof(SimpleLocaleKeyFactory, id) == 88 && offsetof(SimpleLocaleKeyFactory, kind) == 152);
}

extern "C" {
void APS5_VABI _ZN6icu_689LocaleKeyC1ERKNS_13UnicodeStringES3_PS2_i_nid_postfix(Icu::Abi::LocaleKey* self, const Icu::Abi::UnicodeString* primary, const Icu::Abi::UnicodeString* canonical, const Icu::Abi::UnicodeString* fallback, std::int32_t kind);
void APS5_VABI _ZN6icu_689LocaleKeyC2ERKNS_13UnicodeStringES3_PS2_i_nid_postfix(Icu::Abi::LocaleKey* self, const Icu::Abi::UnicodeString* primary, const Icu::Abi::UnicodeString* canonical, const Icu::Abi::UnicodeString* fallback, std::int32_t kind);
void APS5_VABI _ZN6icu_689LocaleKeyD0Ev_nid_postfix(Icu::Abi::LocaleKey* self);
void APS5_VABI _ZN6icu_689LocaleKeyD1Ev_nid_postfix(Icu::Abi::LocaleKey* self);
void APS5_VABI _ZN6icu_689LocaleKeyD2Ev_nid_postfix(Icu::Abi::LocaleKey* self);
void* APS5_VABI _ZN6icu_689LocaleKey16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_689LocaleKey17getDynamicClassIDEv_nid_postfix(const Icu::Abi::LocaleKey* self);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD0Ev_nid_postfix(Icu::Abi::LocaleKeyFactory* self);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD1Ev_nid_postfix(Icu::Abi::LocaleKeyFactory* self);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD2Ev_nid_postfix(Icu::Abi::LocaleKeyFactory* self);
void* APS5_VABI _ZN6icu_6816LocaleKeyFactory16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6816LocaleKeyFactory17getDynamicClassIDEv_nid_postfix(const Icu::Abi::LocaleKeyFactory* self);
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD0Ev_nid_postfix(Icu::Abi::SimpleLocaleKeyFactory* self);
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD1Ev_nid_postfix(Icu::Abi::SimpleLocaleKeyFactory* self);
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD2Ev_nid_postfix(Icu::Abi::SimpleLocaleKeyFactory* self);
void* APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactory16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory17getDynamicClassIDEv_nid_postfix(const Icu::Abi::SimpleLocaleKeyFactory* self);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey11canonicalIDERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::LocaleKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey9currentIDERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::LocaleKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::LocaleKey* self, Icu::Abi::UnicodeString* result);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey6prefixERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::LocaleKey* self, Icu::Abi::UnicodeString* result);
std::int32_t APS5_VABI _ZNK6icu_689LocaleKey4kindEv_nid_postfix(const Icu::Abi::LocaleKey* self);
std::int8_t APS5_VABI _ZN6icu_689LocaleKey8fallbackEv_nid_postfix(Icu::Abi::LocaleKey* self);
std::int8_t APS5_VABI _ZNK6icu_689LocaleKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix(const Icu::Abi::LocaleKey* self, const Icu::Abi::UnicodeString* id);
Icu::Abi::LocaleKey* APS5_VABI _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* primary, const Icu::Abi::UnicodeString* fallback, std::int32_t* status);
Icu::Abi::LocaleKey* APS5_VABI _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_iR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* primary, const Icu::Abi::UnicodeString* fallback, std::int32_t kind, std::int32_t* status);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC1Ei_nid_postfix(Icu::Abi::LocaleKeyFactory* self, std::int32_t coverage);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC1EiRKNS_13UnicodeStringE_nid_postfix(Icu::Abi::LocaleKeyFactory* self, std::int32_t coverage, const Icu::Abi::UnicodeString* name);
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEii_nid_postfix(Icu::Abi::SimpleLocaleKeyFactory* self, Icu::Abi::Object* object, const Icu::Abi::UnicodeString* id, std::int32_t kind, std::int32_t coverage);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC2Ei_nid_postfix(Icu::Abi::LocaleKeyFactory* self, std::int32_t coverage);
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC2EiRKNS_13UnicodeStringE_nid_postfix(Icu::Abi::LocaleKeyFactory* self, std::int32_t coverage, const Icu::Abi::UnicodeString* name);
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryC2EPNS_7UObjectERKNS_13UnicodeStringEii_nid_postfix(Icu::Abi::SimpleLocaleKeyFactory* self, Icu::Abi::Object* object, const Icu::Abi::UnicodeString* id, std::int32_t kind, std::int32_t coverage);
const Icu::Abi::Hashtable* APS5_VABI _ZNK6icu_6816LocaleKeyFactory15getSupportedIDsER10UErrorCode_nid_postfix(const Icu::Abi::LocaleKeyFactory* self, std::int32_t* status);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6816LocaleKeyFactory12handleCreateERKNS_6LocaleEiPKNS_10ICUServiceER10UErrorCode_nid_postfix(const Icu::Abi::LocaleKeyFactory* self, const void* locale, std::int32_t kind, const Icu::Abi::ICUService* service, std::int32_t* status);
std::int8_t APS5_VABI _ZNK6icu_6816LocaleKeyFactory10handlesKeyERKNS_13ICUServiceKeyER10UErrorCode_nid_postfix(const Icu::Abi::LocaleKeyFactory* self, const Icu::Abi::ICUServiceKey* key, std::int32_t* status);
void APS5_VABI _ZNK6icu_6816LocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const Icu::Abi::LocaleKeyFactory* self, Icu::Abi::Hashtable* result, std::int32_t* status);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix(const Icu::Abi::SimpleLocaleKeyFactory* self, const Icu::Abi::ICUServiceKey* key, const Icu::Abi::ICUService* service, std::int32_t* status);
void APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const Icu::Abi::SimpleLocaleKeyFactory* self, Icu::Abi::Hashtable* result, std::int32_t* status);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813LocaleUtility21canonicalLocaleStringEPKNS_13UnicodeStringERS1__nid_postfix(const Icu::Abi::UnicodeString* id, Icu::Abi::UnicodeString* result);
std::int8_t APS5_VABI _ZN6icu_6813LocaleUtility12isFallbackOfERKNS_13UnicodeStringES3__nid_postfix(const Icu::Abi::UnicodeString* root, const Icu::Abi::UnicodeString* child);
}
