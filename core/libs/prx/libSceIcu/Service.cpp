// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU service: null pointer");
    return *value;
}
const TypeInfo* NotifierType() {
    ICUNotifier object{};
    _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(&object);
    return reinterpret_cast<const TypeInfo* const*>(object.vtable)[-1];
}
const DerivedTypeInfo ServiceType{{SingleTypeTable_nid_no_patch(), "N6icu_6810ICUServiceE"}, NotifierType()};
struct Table { std::ptrdiff_t offset; const TypeInfo* type; std::array<GuestFunction, 19> slots; };
static_assert(sizeof(Table) == 168 && alignof(Table) == 8 && offsetof(Table, slots) == 16);
void APS5_VABI Abstract_nid_no_patch() { throw std::invalid_argument("ICU service: abstract virtual call"); }
const Table ServiceTable{0, &ServiceType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUServiceD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUServiceD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService15acceptsListenerERKNS_13EventListenerE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService14notifyListenerERNS_13EventListenerE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService16registerInstanceEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService15registerFactoryEPNS_17ICUServiceFactoryER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService10unregisterEPKvR10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService5resetEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService9isDefaultEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService9createKeyEPKNS_13UnicodeStringER10UErrorCode_nid_postfix),
    Abstract_nid_no_patch,
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService19createSimpleFactoryEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService21reInitializeFactoriesEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6810ICUService13handleDefaultERKNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6810ICUService11clearCachesEv_nid_postfix),
}}};
struct LocalString {
    UnicodeString text{};
    LocalString() { text.Init(); }
    ~LocalString() { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text); }
};
struct LocalKey {
    ICUServiceKey* key;
    ~LocalKey() {
        using Delete = void (APS5_VABI *)(ICUServiceKey*);
        if (key) Virtual_nid_no_patch<Delete>(key, 1)(key);
    }
};
void DeleteObject(Object* object) {
    using Delete = void (APS5_VABI *)(Object*);
    if (object) Virtual_nid_no_patch<Delete>(object, 1)(object);
}
void Construct(ICUService* self, const UnicodeString* name) {
    Require(self);
    _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(reinterpret_cast<ICUNotifier*>(self));
    self->vtable = ServiceTable.slots.data();
    if (name) _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->name, name);
    else self->name.Init();
    self->timestamp = 0;
    self->factories = nullptr;
    self->serviceCache = nullptr;
    self->idCache = nullptr;
    self->displayNameCache = nullptr;
}
}

extern "C" {
void APS5_VABI _ZN6icu_6810ICUServiceC2Ev_nid_postfix(ICUService* self) { Construct(self, nullptr); }
void APS5_VABI _ZN6icu_6810ICUServiceC2ERKNS_13UnicodeStringE_nid_postfix(ICUService* self, const UnicodeString* name) { Require(name); Construct(self, name); }
std::int32_t APS5_VABI _ZNK6icu_6810ICUService14countFactoriesEv_nid_postfix(const ICUService* self) { const auto* vector = Require(self).factories; return vector ? vector->count : 0; }
std::int8_t APS5_VABI _ZNK6icu_6810ICUService9isDefaultEv_nid_postfix(const ICUService* self) { return _ZNK6icu_6810ICUService14countFactoriesEv_nid_postfix(self) == 0; }
std::int32_t APS5_VABI _ZNK6icu_6810ICUService12getTimestampEv_nid_postfix(const ICUService* self) { return std::bit_cast<std::int32_t>(Require(self).timestamp); }
UnicodeString* APS5_VABI _ZNK6icu_6810ICUService7getNameERNS_13UnicodeStringE_nid_postfix(const ICUService* self, UnicodeString* result) {
    const auto& name = Require(self).name;
    return _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(result, &name, 0, name.Length());
}
ICUServiceKey* APS5_VABI _ZNK6icu_6810ICUService9createKeyEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService*, const UnicodeString* id, std::int32_t* status) {
    if (Require(status) > 0 || !id) return nullptr;
    auto* key = static_cast<ICUServiceKey*>(Allocate_nid_no_patch(sizeof(ICUServiceKey)));
    if (key) _ZN6icu_6813ICUServiceKeyC2ERKNS_13UnicodeStringE_nid_postfix(key, id);
    return key;
}
ICUServiceFactory* APS5_VABI _ZN6icu_6810ICUService19createSimpleFactoryEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix(ICUService*, Object* instance, const UnicodeString* id, std::int8_t visible, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    if (!instance || (Require(id).Flags() & UnicodeString::IsBogus)) { *status = 1; return nullptr; }
    auto* factory = static_cast<SimpleFactory*>(Allocate_nid_no_patch(sizeof(SimpleFactory)));
    if (factory) _ZN6icu_6813SimpleFactoryC2EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(factory, instance, id, visible);
    return reinterpret_cast<ICUServiceFactory*>(factory);
}
Object* APS5_VABI _ZNK6icu_6810ICUService13handleDefaultERKNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService*, const ICUServiceKey*, UnicodeString*, std::int32_t*) { return nullptr; }
void APS5_VABI _ZN6icu_6810ICUService21reInitializeFactoriesEv_nid_postfix(ICUService* self) {
    if (Require(self).factories) _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(self->factories);
}
void APS5_VABI _ZN6icu_6810ICUService17clearServiceCacheEv_nid_postfix(ICUService* self) {
    auto& object = Require(self);
    if (object.serviceCache) { if (object.serviceCache->hash) uhash_close_nid_postfix(object.serviceCache->hash); Free_nid_no_patch(object.serviceCache); }
    object.serviceCache = nullptr;
}
Object* APS5_VABI _ZNK6icu_6810ICUService3getERKNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService* self, const UnicodeString* id, std::int32_t* status) {
    return _ZNK6icu_6810ICUService3getERKNS_13UnicodeStringEPS1_R10UErrorCode_nid_postfix(self, id, nullptr, status);
}
Object* APS5_VABI _ZNK6icu_6810ICUService3getERKNS_13UnicodeStringEPS1_R10UErrorCode_nid_postfix(const ICUService* self, const UnicodeString* id, UnicodeString* actual, std::int32_t* status) {
    Require(id); Require(status);
    using Create = ICUServiceKey* (APS5_VABI *)(const ICUService*, const UnicodeString*, std::int32_t*);
    LocalKey key{Virtual_nid_no_patch<Create>(self, 13)(self, id, status)};
    if (!key.key) return nullptr;
    using Get = Object* (APS5_VABI *)(const ICUService*, ICUServiceKey*, UnicodeString*, std::int32_t*);
    return Virtual_nid_no_patch<Get>(self, 7)(self, key.key, actual, status);
}
Object* APS5_VABI _ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyER10UErrorCode_nid_postfix(const ICUService* self, ICUServiceKey* key, std::int32_t* status) {
    Require(key); Require(status);
    using Get = Object* (APS5_VABI *)(const ICUService*, ICUServiceKey*, UnicodeString*, std::int32_t*);
    return Virtual_nid_no_patch<Get>(self, 7)(self, key, nullptr, status);
}
const void* APS5_VABI _ZN6icu_6810ICUService16registerInstanceEPNS_7UObjectERKNS_13UnicodeStringER10UErrorCode_nid_postfix(ICUService* self, Object* instance, const UnicodeString* id, std::int32_t* status) {
    Require(id); Require(status);
    using Register = const void* (APS5_VABI *)(ICUService*, Object*, const UnicodeString*, std::int8_t, std::int32_t*);
    return Virtual_nid_no_patch<Register>(self, 8)(self, instance, id, 1, status);
}
const void* APS5_VABI _ZN6icu_6810ICUService16registerInstanceEPNS_7UObjectERKNS_13UnicodeStringEaR10UErrorCode_nid_postfix(ICUService* self, Object* instance, const UnicodeString* id, std::int8_t visible, std::int32_t* status) {
    Require(id); Require(status);
    using Create = ICUServiceKey* (APS5_VABI *)(const ICUService*, const UnicodeString*, std::int32_t*);
    LocalKey key{Virtual_nid_no_patch<Create>(self, 13)(self, id, status)};
    if (key.key) {
        LocalString canonical;
        using Canonical = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
        Virtual_nid_no_patch<Canonical>(key.key, 4)(key.key, &canonical.text);
        using Delete = void (APS5_VABI *)(ICUServiceKey*);
        Virtual_nid_no_patch<Delete>(key.key, 1)(key.key); key.key = nullptr;
        using Factory = ICUServiceFactory* (APS5_VABI *)(ICUService*, Object*, const UnicodeString*, std::int8_t, std::int32_t*);
        auto* factory = Virtual_nid_no_patch<Factory>(self, 15)(self, instance, &canonical.text, visible, status);
        if (factory) {
            using Register = const void* (APS5_VABI *)(ICUService*, ICUServiceFactory*, std::int32_t*);
            return Virtual_nid_no_patch<Register>(self, 9)(self, factory, status);
        }
    }
    DeleteObject(instance);
    return nullptr;
}
}
