// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <limits>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU service key: null pointer");
    return *value;
}
char KeyId;
const DerivedTypeInfo KeyType{{SingleTypeTable_nid_no_patch(), "N6icu_6813ICUServiceKeyE"}, Rtti_nid_no_patch(Class::Object)};
struct Table { std::ptrdiff_t offset; const TypeInfo* type; std::array<GuestFunction, 10> slots; };
static_assert(sizeof(Table) == 96 && alignof(Table) == 8 && offsetof(Table, slots) == 16);
const Table KeyTable{0, &KeyType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6813ICUServiceKeyD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6813ICUServiceKeyD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey17getDynamicClassIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey5getIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey11canonicalIDERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey9currentIDERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6813ICUServiceKey8fallbackEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey6prefixERNS_13UnicodeStringE_nid_postfix),
}}};
void Construct(ICUServiceKey* self, const UnicodeString* id) {
    Require(self).vtable = KeyTable.slots.data();
    Require(id);
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->id, id);
}
void Destroy(ICUServiceKey* self) {
    Require(self).vtable = KeyTable.slots.data();
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->id);
}
std::int32_t Delimiter(const UnicodeString* text) {
    static constexpr char16_t Slash = u'/';
    return _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(text, &Slash, 0, 1, 0, Require(text).Length());
}
}

extern "C" {
void APS5_VABI _ZN6icu_6813ICUServiceKeyC1ERKNS_13UnicodeStringE_nid_postfix(ICUServiceKey* self, const UnicodeString* id) { Construct(self, id); }
void APS5_VABI _ZN6icu_6813ICUServiceKeyC2ERKNS_13UnicodeStringE_nid_postfix(ICUServiceKey* self, const UnicodeString* id) { Construct(self, id); }
void APS5_VABI _ZN6icu_6813ICUServiceKeyD1Ev_nid_postfix(ICUServiceKey* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6813ICUServiceKeyD2Ev_nid_postfix(ICUServiceKey* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6813ICUServiceKeyD0Ev_nid_postfix(ICUServiceKey* self) { Destroy(self); Free_nid_no_patch(self); }
void* APS5_VABI _ZN6icu_6813ICUServiceKey16getStaticClassIDEv_nid_postfix() { return &KeyId; }
void* APS5_VABI _ZNK6icu_6813ICUServiceKey17getDynamicClassIDEv_nid_postfix(const ICUServiceKey*) { return &KeyId; }
const UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey5getIDEv_nid_postfix(const ICUServiceKey* self) { return &Require(self).id; }
UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey11canonicalIDERNS_13UnicodeStringE_nid_postfix(const ICUServiceKey* self, UnicodeString* result) {
    const auto& id = Require(self).id;
    return _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(result, &id, 0, id.Length());
}
UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey9currentIDERNS_13UnicodeStringE_nid_postfix(const ICUServiceKey* self, UnicodeString* result) {
    using Function = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
    return Virtual_nid_no_patch<Function>(self, 4)(self, result);
}
UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(const ICUServiceKey* self, UnicodeString* result) {
    using Function = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
    Virtual_nid_no_patch<Function>(self, 9)(self, result);
    static constexpr char16_t Slash = u'/';
    _ZN6icu_6813UnicodeString8doAppendEPKDsii_nid_postfix(result, &Slash, 0, 1);
    return Virtual_nid_no_patch<Function>(self, 5)(self, result);
}
std::int8_t APS5_VABI _ZN6icu_6813ICUServiceKey8fallbackEv_nid_postfix(ICUServiceKey*) { return 0; }
std::int8_t APS5_VABI _ZNK6icu_6813ICUServiceKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix(const ICUServiceKey* self, const UnicodeString* id) {
    return _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(id, &Require(self).id);
}
UnicodeString* APS5_VABI _ZNK6icu_6813ICUServiceKey6prefixERNS_13UnicodeStringE_nid_postfix(const ICUServiceKey*, UnicodeString* result) { Require(result); return result; }
UnicodeString* APS5_VABI _ZN6icu_6813ICUServiceKey11parsePrefixERNS_13UnicodeStringE_nid_postfix(UnicodeString* result) {
    const auto position = Delimiter(result);
    if (position <= 0) {
        if (result->Flags() & UnicodeString::IsBogus) result->Empty();
        else result->SetLength(0);
    } else {
        _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(result, position, std::numeric_limits<std::int32_t>::max(), nullptr, 0, 0);
    }
    return result;
}
UnicodeString* APS5_VABI _ZN6icu_6813ICUServiceKey11parseSuffixERNS_13UnicodeStringE_nid_postfix(UnicodeString* result) {
    const auto position = Delimiter(result);
    if (position >= 0) _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(result, 0, position + 1, nullptr, 0, 0);
    return result;
}
}
