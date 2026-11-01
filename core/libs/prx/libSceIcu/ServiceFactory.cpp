// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU service factory: null pointer");
    return *value;
}
struct LocalString {
    UnicodeString text{};
    LocalString() { text.Init(); }
    ~LocalString() { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text); }
};
char FactoryId;
const DerivedTypeInfo FactoryType{{SingleTypeTable_nid_no_patch(), "N6icu_6817ICUServiceFactoryE"}, Rtti_nid_no_patch(Class::Object)};
const DerivedTypeInfo SimpleType{{SingleTypeTable_nid_no_patch(), "N6icu_6813SimpleFactoryE"}, &FactoryType.type};
struct Table { std::ptrdiff_t offset; const TypeInfo* type; std::array<GuestFunction, 6> slots; };
static_assert(sizeof(Table) == 64 && alignof(Table) == 8 && offsetof(Table, slots) == 16);
void APS5_VABI Abstract_nid_no_patch() { throw std::invalid_argument("ICU service factory: abstract virtual call"); }
const Table FactoryTable{0, &FactoryType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6817ICUServiceFactoryD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6817ICUServiceFactoryD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix),
    Abstract_nid_no_patch, Abstract_nid_no_patch, Abstract_nid_no_patch
}}};
const Table SimpleTable{0, &SimpleType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6813SimpleFactoryD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6813SimpleFactoryD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813SimpleFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813SimpleFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813SimpleFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix)
}}};
void Construct(SimpleFactory* self, Object* instance, const UnicodeString* id, std::int8_t visible) {
    Require(self).vtable = SimpleTable.slots.data();
    self->instance = instance;
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->id, id);
    self->visible = visible;
}
void Destroy(SimpleFactory* self) {
    Require(self).vtable = SimpleTable.slots.data();
    using Delete = void (APS5_VABI *)(Object*);
    if (self->instance) Virtual_nid_no_patch<Delete>(self->instance, 1)(self->instance);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->id);
    _ZN6icu_6817ICUServiceFactoryD2Ev_nid_postfix(reinterpret_cast<ICUServiceFactory*>(self));
}
}

namespace Icu::Abi {
void PutServiceId_nid_no_patch(Hashtable* table, const UnicodeString* id, const void* value, std::int32_t* status) {
    Require(table); Require(status); Require(id);
    auto* copy = static_cast<UnicodeString*>(Allocate_nid_no_patch(sizeof(UnicodeString)));
    if (copy) _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(copy, id);
    uhash_put_nid_postfix(table->hash, copy, const_cast<void*>(value), status);
}
}

extern "C" {
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD1Ev_nid_postfix(ICUServiceFactory* self) { Require(self).vtable = FactoryTable.slots.data(); }
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD2Ev_nid_postfix(ICUServiceFactory* self) { Require(self).vtable = FactoryTable.slots.data(); }
void APS5_VABI _ZN6icu_6817ICUServiceFactoryD0Ev_nid_postfix(ICUServiceFactory* self) { Require(self).vtable = FactoryTable.slots.data(); Free_nid_no_patch(self); }
void APS5_VABI _ZN6icu_6813SimpleFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(SimpleFactory* self, Object* instance, const UnicodeString* id, std::int8_t visible) { Construct(self, instance, id, visible); }
void APS5_VABI _ZN6icu_6813SimpleFactoryC2EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(SimpleFactory* self, Object* instance, const UnicodeString* id, std::int8_t visible) { Construct(self, instance, id, visible); }
void APS5_VABI _ZN6icu_6813SimpleFactoryD1Ev_nid_postfix(SimpleFactory* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6813SimpleFactoryD2Ev_nid_postfix(SimpleFactory* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6813SimpleFactoryD0Ev_nid_postfix(SimpleFactory* self) { Destroy(self); Free_nid_no_patch(self); }
void* APS5_VABI _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix() { return &FactoryId; }
void* APS5_VABI _ZNK6icu_6813SimpleFactory17getDynamicClassIDEv_nid_postfix(const SimpleFactory*) { return &FactoryId; }
Object* APS5_VABI _ZNK6icu_6813SimpleFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix(const SimpleFactory* self, const ICUServiceKey* key, const ICUService* service, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    LocalString temp;
    using Current = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
    const auto* current = Virtual_nid_no_patch<Current>(key, 5)(key, &temp.text);
    const auto equal = _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&Require(self).id, current);
    if (!equal) return nullptr;
    using Clone = Object* (APS5_VABI *)(const ICUService*, Object*);
    return Virtual_nid_no_patch<Clone>(service, 14)(service, self->instance);
}
void APS5_VABI _ZNK6icu_6813SimpleFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const SimpleFactory* self, Hashtable* result, std::int32_t* status) {
    const auto& object = Require(self);
    if (object.visible) PutServiceId_nid_no_patch(result, &object.id, self, status);
    else uhash_remove_nid_postfix(Require(result).hash, &object.id);
}
UnicodeString* APS5_VABI _ZNK6icu_6813SimpleFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix(const SimpleFactory* self, const UnicodeString* id, const void*, UnicodeString* result) {
    const auto& object = Require(self);
    if (object.visible && _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&object.id, id)) return _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(result, &object.id);
    _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(result);
    return result;
}
}
