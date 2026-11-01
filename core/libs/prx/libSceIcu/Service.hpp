// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#pragma once
#include "ServiceFactory.hpp"
#include "ServiceNotifier.hpp"

namespace Icu::Abi {
struct ICUService {
    const GuestFunction* vtable;
    Vector* listeners;
    UnicodeString name;
    std::uint32_t timestamp;
    Vector* factories;
    Hashtable* serviceCache;
    Hashtable* idCache;
    void* displayNameCache;
};
static_assert(sizeof(ICUService) == 120 && alignof(ICUService) == 8);
static_assert(offsetof(ICUService, vtable) == 0 && offsetof(ICUService, listeners) == 8);
static_assert(offsetof(ICUService, name) == 16 && offsetof(ICUService, timestamp) == 80);
static_assert(offsetof(ICUService, factories) == 88 && offsetof(ICUService, serviceCache) == 96);
static_assert(offsetof(ICUService, idCache) == 104 && offsetof(ICUService, displayNameCache) == 112);
}

extern "C" {
void APS5_VABI _ZN6icu_6810ICUServiceC2Ev_nid_postfix(Icu::Abi::ICUService* self);
void APS5_VABI _ZN6icu_6810ICUServiceC2ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::ICUService* self, const Icu::Abi::UnicodeString* name);
std::int8_t APS5_VABI _ZNK6icu_6810ICUService9isDefaultEv_nid_postfix(const Icu::Abi::ICUService* self);
std::int32_t APS5_VABI _ZNK6icu_6810ICUService14countFactoriesEv_nid_postfix(const Icu::Abi::ICUService* self);
std::int32_t APS5_VABI _ZNK6icu_6810ICUService12getTimestampEv_nid_postfix(const Icu::Abi::ICUService* self);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6810ICUService7getNameERNS_13UnicodeStringE_nid_postfix(const Icu::Abi::ICUService* self, Icu::Abi::UnicodeString* result);
Icu::Abi::ICUServiceKey* APS5_VABI _ZNK6icu_6810ICUService9createKeyEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(const Icu::Abi::ICUService* self, const Icu::Abi::UnicodeString* id, std::int32_t* status);
Icu::Abi::ICUServiceFactory* APS5_VABI _ZN6icu_6810ICUService19createSimpleFactoryEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix(Icu::Abi::ICUService* self, Icu::Abi::Object* instance, const Icu::Abi::UnicodeString* id, std::int8_t visible, std::int32_t* status);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6810ICUService13handleDefaultERKNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix(const Icu::Abi::ICUService* self, const Icu::Abi::ICUServiceKey* key, Icu::Abi::UnicodeString* actual, std::int32_t* status);
void APS5_VABI _ZN6icu_6810ICUService21reInitializeFactoriesEv_nid_postfix(Icu::Abi::ICUService* self);
void APS5_VABI _ZN6icu_6810ICUService17clearServiceCacheEv_nid_postfix(Icu::Abi::ICUService* self);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6810ICUService3getERKNS_13UnicodeStringER10UErrorCode_nid_postfix(const Icu::Abi::ICUService* self, const Icu::Abi::UnicodeString* id, std::int32_t* status);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6810ICUService3getERKNS_13UnicodeStringEPS1_R10UErrorCode_nid_postfix(const Icu::Abi::ICUService* self, const Icu::Abi::UnicodeString* id, Icu::Abi::UnicodeString* actual, std::int32_t* status);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyER10UErrorCode_nid_postfix(const Icu::Abi::ICUService* self, Icu::Abi::ICUServiceKey* key, std::int32_t* status);
const void* APS5_VABI _ZN6icu_6810ICUService16registerInstanceEPNS_7UObjectERKNS_13UnicodeStringER10UErrorCode_nid_postfix(Icu::Abi::ICUService* self, Icu::Abi::Object* instance, const Icu::Abi::UnicodeString* id, std::int32_t* status);
const void* APS5_VABI _ZN6icu_6810ICUService16registerInstanceEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix(Icu::Abi::ICUService* self, Icu::Abi::Object* instance, const Icu::Abi::UnicodeString* id, std::int8_t visible, std::int32_t* status);
}
