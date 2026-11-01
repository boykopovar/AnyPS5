// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2002-2014, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2001-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <limits>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) { if (!value) throw std::invalid_argument("ICU service locale: null pointer"); return *value; }
struct LocalString { UnicodeString text{}; LocalString() { text.Init(); } ~LocalString() { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text); } };
std::array<char, 3> Ids{};
const TypeInfo* KeyBaseType() { UnicodeString id{}; id.Init(); ICUServiceKey key{}; _ZN6icu_6813ICUServiceKeyC2ERKNS_13UnicodeStringE_nid_postfix(&key, &id); const auto* type = reinterpret_cast<const TypeInfo* const*>(key.vtable)[-1]; _ZN6icu_6813ICUServiceKeyD2Ev_nid_postfix(&key); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id); return type; }
const TypeInfo* FactoryBaseType() { ICUServiceFactory factory{}; _ZN6icu_6817ICUServiceFactoryD2Ev_nid_postfix(&factory); return reinterpret_cast<const TypeInfo* const*>(factory.vtable)[-1]; }
const DerivedTypeInfo KeyType{{SingleTypeTable_nid_no_patch(), "N6icu_689LocaleKeyE"}, KeyBaseType()};
const DerivedTypeInfo FactoryType{{SingleTypeTable_nid_no_patch(), "N6icu_6816LocaleKeyFactoryE"}, FactoryBaseType()};
const DerivedTypeInfo SimpleType{{SingleTypeTable_nid_no_patch(), "N6icu_6822SimpleLocaleKeyFactoryE"}, &FactoryType.type};
template<std::size_t TCount> struct Table { std::ptrdiff_t offset; const TypeInfo* type; std::array<GuestFunction, TCount> slots; };
static_assert(sizeof(Table<13>) == 120 && alignof(Table<13>) == 8 && offsetof(Table<13>, slots) == 16);
static_assert(sizeof(Table<9>) == 88 && alignof(Table<9>) == 8 && offsetof(Table<9>, slots) == 16);
const Table<13> KeyTable{0, &KeyType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_689LocaleKeyD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_689LocaleKeyD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey17getDynamicClassIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813ICUServiceKey5getIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey11canonicalIDERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey9currentIDERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_689LocaleKey8fallbackEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey6prefixERNS_13UnicodeStringE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey4kindEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey15canonicalLocaleERNS_6LocaleE_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_689LocaleKey13currentLocaleERNS_6LocaleE_nid_postfix),
}}};
const Table<9> FactoryTable{0, &FactoryType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6816LocaleKeyFactoryD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6816LocaleKeyFactoryD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory17getDynamicClassIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory10handlesKeyERKNS_13ICUServiceKeyER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory12handleCreateERKNS_6LocaleEiPKNS_10ICUServiceER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory15getSupportedIDsER10UErrorCode_nid_postfix),
}}};
const Table<9> SimpleTable{0, &SimpleType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6822SimpleLocaleKeyFactoryD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6822SimpleLocaleKeyFactoryD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6822SimpleLocaleKeyFactory17getDynamicClassIDEv_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6822SimpleLocaleKeyFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6822SimpleLocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory10handlesKeyERKNS_13ICUServiceKeyER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory12handleCreateERKNS_6LocaleEiPKNS_10ICUServiceER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6816LocaleKeyFactory15getSupportedIDsER10UErrorCode_nid_postfix),
}}};
void RemoveTail(UnicodeString* text, std::int32_t index) {
    if (index == 0) { if (text->Flags() & UnicodeString::IsBogus) text->Empty(); else text->SetLength(0); }
    else _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(text, index, std::numeric_limits<std::int32_t>::max(), nullptr, 0, 0);
}
std::int32_t Index(const UnicodeString* text, char16_t unit) { return _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(text, &unit, 0, 1, 0, Require(text).Length()); }
void ConstructKey(LocaleKey* self, const UnicodeString* primary, const UnicodeString* canonical, const UnicodeString* fallback, std::int32_t kind) {
    Require(self); Require(primary); Require(canonical);
    _ZN6icu_6813ICUServiceKeyC2ERKNS_13UnicodeStringE_nid_postfix(reinterpret_cast<ICUServiceKey*>(self), primary);
    self->vtable = KeyTable.slots.data(); self->kind = kind;
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->primaryId, canonical);
    self->fallbackId.Init(); self->currentId.Init(); self->fallbackId.Bogus();
    if (self->primaryId.Length() && fallback && !_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&self->primaryId, fallback)) _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&self->fallbackId, fallback);
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&self->currentId, &self->primaryId);
}
void DestroyKey(LocaleKey* self) {
    Require(self).vtable = KeyTable.slots.data();
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->currentId); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->fallbackId); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->primaryId);
    _ZN6icu_6813ICUServiceKeyD2Ev_nid_postfix(reinterpret_cast<ICUServiceKey*>(self));
}
void ConstructFactory(LocaleKeyFactory* self, std::int32_t coverage, const UnicodeString* name) {
    Require(self).vtable = FactoryTable.slots.data();
    if (name) _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->name, name); else self->name.Init();
    self->coverage = coverage;
}
void DestroyFactory(LocaleKeyFactory* self) {
    Require(self).vtable = FactoryTable.slots.data(); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->name);
    _ZN6icu_6817ICUServiceFactoryD2Ev_nid_postfix(reinterpret_cast<ICUServiceFactory*>(self));
}
void ConstructSimple(SimpleLocaleKeyFactory* self, Object* object, const UnicodeString* id, std::int32_t kind, std::int32_t coverage) {
    Require(self); Require(id); ConstructFactory(reinterpret_cast<LocaleKeyFactory*>(self), coverage, nullptr);
    self->vtable = SimpleTable.slots.data(); self->object = object;
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->id, id); self->kind = kind;
}
void DestroySimple(SimpleLocaleKeyFactory* self) {
    Require(self).vtable = SimpleTable.slots.data();
    using Delete = void (APS5_VABI *)(Object*);
    if (self->object) Virtual_nid_no_patch<Delete>(self->object, 1)(self->object);
    self->object = nullptr; _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&self->id); DestroyFactory(reinterpret_cast<LocaleKeyFactory*>(self));
}
struct ServiceMultipleType { TypeInfo type; std::uint32_t flags; std::uint32_t count; };
struct ServiceBaseType { const TypeInfo* type; std::int64_t offsetFlags; };
static_assert(sizeof(ServiceMultipleType) == 24 && alignof(ServiceMultipleType) == 8 && offsetof(ServiceMultipleType, count) == 20);
static_assert(sizeof(ServiceBaseType) == 16 && alignof(ServiceBaseType) == 8 && offsetof(ServiceBaseType, offsetFlags) == 8);
bool HasLocaleKey(const TypeInfo* type) {
    if (!type || !type->name || !type->vtable) throw std::invalid_argument("ICU service locale: missing RTTI");
    if (std::strcmp(type->name, "N6icu_689LocaleKeyE") == 0) return true;
    const auto* category = reinterpret_cast<const TypeInfo* const*>(type->vtable)[-1];
    if (!category || !category->name) throw std::invalid_argument("ICU service locale: missing RTTI category");
    if (std::strcmp(category->name, "N10__cxxabiv120__si_class_type_infoE") == 0) return HasLocaleKey(reinterpret_cast<const DerivedTypeInfo*>(type)->base);
    if (std::strcmp(category->name, "N10__cxxabiv121__vmi_class_type_infoE") == 0) {
        const auto* multiple = reinterpret_cast<const ServiceMultipleType*>(type);
        const auto* bases = reinterpret_cast<const ServiceBaseType*>(multiple + 1);
        for (std::uint32_t i = 0; i < multiple->count; ++i) if (HasLocaleKey(bases[i].type)) return true;
    }
    return false;
}
}

extern "C" {
void APS5_VABI _ZN6icu_689LocaleKeyC1ERKNS_13UnicodeStringES3_PS2_i_nid_postfix(LocaleKey* self, const UnicodeString* primary, const UnicodeString* canonical, const UnicodeString* fallback, std::int32_t kind) { ConstructKey(self, primary, canonical, fallback, kind); }
void APS5_VABI _ZN6icu_689LocaleKeyC2ERKNS_13UnicodeStringES3_PS2_i_nid_postfix(LocaleKey* self, const UnicodeString* primary, const UnicodeString* canonical, const UnicodeString* fallback, std::int32_t kind) { ConstructKey(self, primary, canonical, fallback, kind); }
void APS5_VABI _ZN6icu_689LocaleKeyD1Ev_nid_postfix(LocaleKey* self) { DestroyKey(self); }
void APS5_VABI _ZN6icu_689LocaleKeyD2Ev_nid_postfix(LocaleKey* self) { DestroyKey(self); }
void APS5_VABI _ZN6icu_689LocaleKeyD0Ev_nid_postfix(LocaleKey* self) { DestroyKey(self); Free_nid_no_patch(self); }
void* APS5_VABI _ZN6icu_689LocaleKey16getStaticClassIDEv_nid_postfix() { return &Ids[0]; }
void* APS5_VABI _ZNK6icu_689LocaleKey17getDynamicClassIDEv_nid_postfix(const LocaleKey*) { return &Ids[0]; }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD1Ev_nid_postfix(LocaleKeyFactory* self) { DestroyFactory(self); }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD2Ev_nid_postfix(LocaleKeyFactory* self) { DestroyFactory(self); }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryD0Ev_nid_postfix(LocaleKeyFactory* self) { DestroyFactory(self); Free_nid_no_patch(self); }
void* APS5_VABI _ZN6icu_6816LocaleKeyFactory16getStaticClassIDEv_nid_postfix() { return &Ids[1]; }
void* APS5_VABI _ZNK6icu_6816LocaleKeyFactory17getDynamicClassIDEv_nid_postfix(const LocaleKeyFactory*) { return &Ids[1]; }
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD1Ev_nid_postfix(SimpleLocaleKeyFactory* self) { DestroySimple(self); }
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD2Ev_nid_postfix(SimpleLocaleKeyFactory* self) { DestroySimple(self); }
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryD0Ev_nid_postfix(SimpleLocaleKeyFactory* self) { DestroySimple(self); Free_nid_no_patch(self); }
void* APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactory16getStaticClassIDEv_nid_postfix() { return &Ids[2]; }
void* APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory17getDynamicClassIDEv_nid_postfix(const SimpleLocaleKeyFactory*) { return &Ids[2]; }
UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey6prefixERNS_13UnicodeStringE_nid_postfix(const LocaleKey* self, UnicodeString* result) {
    Require(result);
    if (Require(self).kind != -1) {
        char bytes[64]{}; char16_t units[64]{};
        const auto conversion = std::to_chars(bytes, bytes + 64, static_cast<std::uint32_t>(self->kind));
        const auto length = static_cast<std::int32_t>(conversion.ptr - bytes);
        for (std::int32_t i = 0; i < length; ++i) units[i] = static_cast<char16_t>(bytes[i]);
        _ZN6icu_6813UnicodeString8doAppendEPKDsii_nid_postfix(result, units, 0, length);
    }
    return result;
}
std::int32_t APS5_VABI _ZNK6icu_689LocaleKey4kindEv_nid_postfix(const LocaleKey* self) { return Require(self).kind; }
UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey11canonicalIDERNS_13UnicodeStringE_nid_postfix(const LocaleKey* self, UnicodeString* result) { const auto& id = Require(self).primaryId; return _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(result, &id, 0, id.Length()); }
UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey9currentIDERNS_13UnicodeStringE_nid_postfix(const LocaleKey* self, UnicodeString* result) {
    const auto& id = Require(self).currentId; Require(result);
    if (!(id.Flags() & UnicodeString::IsBogus)) _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(result, &id, 0, id.Length());
    return result;
}
UnicodeString* APS5_VABI _ZNK6icu_689LocaleKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(const LocaleKey* self, UnicodeString* result) {
    const auto& id = Require(self).currentId; Require(result);
    if (id.Flags() & UnicodeString::IsBogus) result->Bogus();
    else {
        using Prefix = UnicodeString* (APS5_VABI *)(const LocaleKey*, UnicodeString*);
        auto* output = Virtual_nid_no_patch<Prefix>(self, 9)(self, result);
        static constexpr char16_t Slash = u'/';
        _ZN6icu_6813UnicodeString8doAppendEPKDsii_nid_postfix(output, &Slash, 0, 1);
        _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(output, &id, 0, id.Length());
    }
    return result;
}
std::int8_t APS5_VABI _ZN6icu_689LocaleKey8fallbackEv_nid_postfix(LocaleKey* self) {
    auto& object = Require(self); auto& current = object.currentId;
    if (current.Flags() & UnicodeString::IsBogus) return 0;
    static constexpr char16_t Underscore = u'_';
    const auto index = _ZNK6icu_6813UnicodeString11lastIndexOfEPKDsiiii_nid_postfix(&current, &Underscore, 0, 1, 0, current.Length());
    if (index >= 0) { RemoveTail(&current, index); return 1; }
    if (!(object.fallbackId.Flags() & UnicodeString::IsBogus)) { _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&current, &object.fallbackId); object.fallbackId.Bogus(); return 1; }
    if (current.Length()) { RemoveTail(&current, 0); return 1; }
    current.Bogus(); return 0;
}
std::int8_t APS5_VABI _ZNK6icu_689LocaleKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix(const LocaleKey* self, const UnicodeString* id) {
    LocalString temp; _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&temp.text, id); _ZN6icu_6813ICUServiceKey11parseSuffixERNS_13UnicodeStringE_nid_postfix(&temp.text);
    return _ZN6icu_6813LocaleUtility12isFallbackOfERKNS_13UnicodeStringES3__nid_postfix(&Require(self).primaryId, &temp.text);
}
LocaleKey* APS5_VABI _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(const UnicodeString* primary, const UnicodeString* fallback, std::int32_t* status) { return _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_iR10UErrorCode_nid_postfix(primary, fallback, -1, status); }
LocaleKey* APS5_VABI _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_iR10UErrorCode_nid_postfix(const UnicodeString* primary, const UnicodeString* fallback, std::int32_t kind, std::int32_t* status) {
    if (!primary || Require(status) > 0) return nullptr;
    LocalString canonical; _ZN6icu_6813LocaleUtility21canonicalLocaleStringEPKNS_13UnicodeStringERS1__nid_postfix(primary, &canonical.text);
    auto* key = static_cast<LocaleKey*>(Allocate_nid_no_patch(sizeof(LocaleKey)));
    if (key) ConstructKey(key, primary, &canonical.text, fallback, kind);
    return key;
}
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC1Ei_nid_postfix(LocaleKeyFactory* self, std::int32_t coverage) { ConstructFactory(self, coverage, nullptr); }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC1EiRKNS_13UnicodeStringE_nid_postfix(LocaleKeyFactory* self, std::int32_t coverage, const UnicodeString* name) { Require(name); ConstructFactory(self, coverage, name); }
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEii_nid_postfix(SimpleLocaleKeyFactory* self, Object* object, const UnicodeString* id, std::int32_t kind, std::int32_t coverage) { ConstructSimple(self, object, id, kind, coverage); }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC2Ei_nid_postfix(LocaleKeyFactory* self, std::int32_t coverage) { ConstructFactory(self, coverage, nullptr); }
void APS5_VABI _ZN6icu_6816LocaleKeyFactoryC2EiRKNS_13UnicodeStringE_nid_postfix(LocaleKeyFactory* self, std::int32_t coverage, const UnicodeString* name) { Require(name); ConstructFactory(self, coverage, name); }
void APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactoryC2EPNS_7UObjectERKNS_13UnicodeStringEii_nid_postfix(SimpleLocaleKeyFactory* self, Object* object, const UnicodeString* id, std::int32_t kind, std::int32_t coverage) { ConstructSimple(self, object, id, kind, coverage); }
const Hashtable* APS5_VABI _ZNK6icu_6816LocaleKeyFactory15getSupportedIDsER10UErrorCode_nid_postfix(const LocaleKeyFactory*, std::int32_t*) { return nullptr; }
Object* APS5_VABI _ZNK6icu_6816LocaleKeyFactory12handleCreateERKNS_6LocaleEiPKNS_10ICUServiceER10UErrorCode_nid_postfix(const LocaleKeyFactory*, const void*, std::int32_t, const ICUService*, std::int32_t*) { return nullptr; }
std::int8_t APS5_VABI _ZNK6icu_6816LocaleKeyFactory10handlesKeyERKNS_13ICUServiceKeyER10UErrorCode_nid_postfix(const LocaleKeyFactory* self, const ICUServiceKey* key, std::int32_t* status) {
    using Supported = const Hashtable* (APS5_VABI *)(const LocaleKeyFactory*, std::int32_t*);
    const auto* table = Virtual_nid_no_patch<Supported>(self, 8)(self, status);
    if (!table) return 0;
    LocalString id; using Current = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
    Virtual_nid_no_patch<Current>(key, 5)(key, &id.text);
    return uhash_get_nid_postfix(table->hash, &id.text) != nullptr;
}
void APS5_VABI _ZNK6icu_6816LocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const LocaleKeyFactory* self, Hashtable* result, std::int32_t* status) {
    using Supported = const Hashtable* (APS5_VABI *)(const LocaleKeyFactory*, std::int32_t*);
    const auto* table = Virtual_nid_no_patch<Supported>(self, 8)(self, status);
    if (!table) return;
    const bool visible = (Require(self).coverage & 1) == 0;
    std::int32_t position = -1;
    while (const auto* entry = uhash_nextElement_nid_postfix(table->hash, &position)) {
        const auto* id = static_cast<const UnicodeString*>(entry->key.pointer);
        if (!visible) uhash_remove_nid_postfix(Require(result).hash, id);
        else { PutServiceId_nid_no_patch(result, id, self, status); if (Require(status) > 0) break; }
    }
}
Object* APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix(const SimpleLocaleKeyFactory* self, const ICUServiceKey* key, const ICUService* service, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    Require(key); Require(self);
    if (!HasLocaleKey(reinterpret_cast<const TypeInfo* const*>(key->vtable)[-1])) throw std::invalid_argument("ICU service locale: key is not a LocaleKey");
    using Kind = std::int32_t (APS5_VABI *)(const ICUServiceKey*);
    if (self->kind != -1 && self->kind != Virtual_nid_no_patch<Kind>(key, 10)(key)) return nullptr;
    LocalString id; using Current = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
    Virtual_nid_no_patch<Current>(key, 5)(key, &id.text);
    if (!_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&self->id, &id.text)) return nullptr;
    using Clone = Object* (APS5_VABI *)(const ICUService*, Object*);
    return Virtual_nid_no_patch<Clone>(service, 14)(service, self->object);
}
void APS5_VABI _ZNK6icu_6822SimpleLocaleKeyFactory16updateVisibleIDsERNS_9HashtableER10UErrorCode_nid_postfix(const SimpleLocaleKeyFactory* self, Hashtable* result, std::int32_t* status) {
    if (Require(status) > 0) return;
    const auto& object = Require(self);
    if (object.coverage & 1) uhash_remove_nid_postfix(Require(result).hash, &object.id);
    else PutServiceId_nid_no_patch(result, &object.id, self, status);
}
UnicodeString* APS5_VABI _ZN6icu_6813LocaleUtility21canonicalLocaleStringEPKNS_13UnicodeStringERS1__nid_postfix(const UnicodeString* id, UnicodeString* result) {
    Require(result);
    if (!id) { result->Bogus(); return result; }
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(result, id);
    auto end = Index(result, u'@'); auto n = Index(result, u'.');
    if (n >= 0 && n < end) end = n;
    if (end < 0) end = result->Length();
    n = Index(result, u'_'); if (n < 0) n = end;
    std::int32_t i = 0;
    for (; i < n; ++i) { const auto unit = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(result, i); if (unit >= u'A' && unit <= u'Z') _ZN6icu_6813UnicodeString9setCharAtEiDs_nid_postfix(result, i, unit + 0x20); }
    for (; i < end; ++i) { const auto unit = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(result, i); if (unit >= u'a' && unit <= u'z') _ZN6icu_6813UnicodeString9setCharAtEiDs_nid_postfix(result, i, unit - 0x20); }
    return result;
}
std::int8_t APS5_VABI _ZN6icu_6813LocaleUtility12isFallbackOfERKNS_13UnicodeStringES3__nid_postfix(const UnicodeString* root, const UnicodeString* child) {
    Require(root); Require(child);
    return _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(child, root->Array(), 0, root->Length(), 0, child->Length()) == 0 && (child->Length() == root->Length() || _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(child, root->Length()) == u'_');
}
}
