// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#pragma once
#include "ServiceKey.hpp"
#include "Hash.hpp"

namespace Icu::Abi {
struct ICUService;
struct ICUServiceFactory { const GuestFunction* vtable; };
struct SimpleFactory { const GuestFunction* vtable; Object* instance; UnicodeString id; std::int8_t visible; };
struct Hashtable { Icu::UHashtable* hash; Icu::UHashtable hashObject; };
static_assert(sizeof(ICUServiceFactory) == 8 && alignof(ICUServiceFactory) == 8 && offsetof(ICUServiceFactory, vtable) == 0);
static_assert(sizeof(SimpleFactory) == 88 && alignof(SimpleFactory) == 8);
static_assert(offsetof(SimpleFactory, vtable) == 0 && offsetof(SimpleFactory, instance) == 8);
static_assert(offsetof(SimpleFactory, id) == 16 && offsetof(SimpleFactory, visible) == 80);
static_assert(sizeof(Hashtable) == 88 && alignof(Hashtable) == 8);
static_assert(offsetof(Hashtable, hash) == 0 && offsetof(Hashtable, hashObject) == 8);
void PutServiceId_nid_no_patch(Hashtable* table, const UnicodeString* id, const void* value, std::int32_t* status);
}

extern "C" {
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD0Ev_nid_postfix(Icu::Abi::ICUServiceFactory* self);
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD1Ev_nid_postfix(Icu::Abi::ICUServiceFactory* self);
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD2Ev_nid_postfix(Icu::Abi::ICUServiceFactory* self);
void APS5_VABI _ZN6icu_6813SimpleFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(Icu::Abi::SimpleFactory* self, Icu::Abi::Object* instance, const Icu::Abi::UnicodeString* id, std::int8_t visible);
void APS5_VABI _ZN6icu_6813SimpleFactoryC2EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(Icu::Abi::SimpleFactory* self, Icu::Abi::Object* instance, const Icu::Abi::UnicodeString* id, std::int8_t visible);
void APS5_VABI _ZN6icu_6813SimpleFactoryD0Ev_nid_postfix(Icu::Abi::SimpleFactory* self);
void APS5_VABI _ZN6icu_6813SimpleFactoryD1Ev_nid_postfix(Icu::Abi::SimpleFactory* self);
void APS5_VABI _ZN6icu_6813SimpleFactoryD2Ev_nid_postfix(Icu::Abi::SimpleFactory* self);
void* APS5_VABI _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix(const Icu::Abi::SimpleFactory* self);
Icu::Abi::Object* APS5_VABI _ZNK6icu_6813SimpleFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix(const Icu::Abi::SimpleFactory* self, const Icu::Abi::ICUServiceKey* key, const Icu::Abi::ICUService* service, std::int32_t* status);
void APS5_VABI _ZNK6icu_6813SimpleFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const Icu::Abi::SimpleFactory* self, Icu::Abi::Hashtable* result, std::int32_t* status);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813SimpleFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix(const Icu::Abi::SimpleFactory* self, const Icu::Abi::UnicodeString* id, const void* locale, Icu::Abi::UnicodeString* result);
}
