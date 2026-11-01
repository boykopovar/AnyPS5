// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>
#include <mutex>
#include <string_view>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU service: null pointer");
    return *value;
}
std::recursive_mutex ServiceLock;
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
void APS5_VABI DeleteFactory_nid_no_patch(void* object) {
    DeleteObject(static_cast<Object*>(object));
}
void APS5_VABI DeleteUnicodeStringObj_nid_no_patch(void* obj) {
    if (obj) {
        auto* str = static_cast<UnicodeString*>(obj);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(str);
        Free_nid_no_patch(str);
    }
}
void APS5_VABI DeleteStringPair_nid_no_patch(void* obj) {
    if (obj) {
        auto* pair = static_cast<StringPair*>(obj);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->id);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->displayName);
        Free_nid_no_patch(pair);
    }
}
UnicodeString* CloneString(const UnicodeString* src) {
    if (!src) return nullptr;
    auto* copy = static_cast<UnicodeString*>(Allocate_nid_no_patch(sizeof(UnicodeString)));
    if (copy) _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(copy, src);
    return copy;
}
struct CacheEntry {
    std::int32_t refcount;
    UnicodeString actualDescriptor;
    Object* service;
    CacheEntry(const UnicodeString& desc, Object* serv)
        : refcount(1), actualDescriptor{}, service(serv) {
        _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&actualDescriptor, &desc);
    }
    ~CacheEntry() {
        DeleteObject(service);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&actualDescriptor);
    }
    CacheEntry* Ref() {
        ++refcount;
        return this;
    }
    CacheEntry* Unref() {
        if (--refcount == 0) {
            this->~CacheEntry();
            Free_nid_no_patch(this);
            return nullptr;
        }
        return this;
    }
};
void APS5_VABI CacheDeleter_nid_no_patch(void* obj) {
    if (obj) static_cast<CacheEntry*>(obj)->Unref();
}
static_assert(sizeof(CacheEntry) == 80 && alignof(CacheEntry) == 8);
static_assert(offsetof(CacheEntry, refcount) == 0 && offsetof(CacheEntry, actualDescriptor) == 8 && offsetof(CacheEntry, service) == 72);
struct DNCache {
    alignas(8) char padding[8];
    Hashtable cache;
    Locale locale;
};
static_assert(sizeof(DNCache) == 320 && alignof(DNCache) == 8);
static_assert(offsetof(DNCache, cache) == 8 && offsetof(DNCache, locale) == 96);
void ClearDNCache(void*& dnCache) {
    if (dnCache) {
        auto* cache = static_cast<DNCache*>(dnCache);
        _ZN6icu_686LocaleD1Ev_nid_postfix(&cache->locale);
        if (cache->cache.hash) uhash_close_nid_postfix(cache->cache.hash);
        Free_nid_no_patch(cache);
        dnCache = nullptr;
    }
}
void ClearIdCache(Hashtable*& idCache) {
    if (idCache) {
        if (idCache->hash) uhash_close_nid_postfix(idCache->hash);
        Free_nid_no_patch(idCache);
        idCache = nullptr;
    }
}
void ClearServiceCache(Hashtable*& serviceCache) {
    if (serviceCache) {
        if (serviceCache->hash) uhash_close_nid_postfix(serviceCache->hash);
        Free_nid_no_patch(serviceCache);
        serviceCache = nullptr;
    }
}
Hashtable* CreateHashtable(std::int32_t* status) {
    if (*status > 0) return nullptr;
    auto* table = static_cast<Hashtable*>(Allocate_nid_no_patch(sizeof(Hashtable)));
    if (!table) return nullptr;
    uhash_init_nid_postfix(&table->hashObject, reinterpret_cast<Icu::HashFunction>(uhash_hashUnicodeString_nid_postfix), reinterpret_cast<Icu::HashComparator>(uhash_compareUnicodeString_nid_postfix), nullptr, status);
    if (*status > 0) { Free_nid_no_patch(table); return nullptr; }
    table->hash = &table->hashObject;
    uhash_setKeyDeleter_nid_postfix(table->hash, DeleteUnicodeStringObj_nid_no_patch);
    return table;
}
Vector* CreateFactoryVector(std::int32_t* status) {
    if (*status > 0) return nullptr;
    auto* vector = static_cast<Vector*>(Allocate_nid_no_patch(sizeof(Vector)));
    if (!vector) throw std::invalid_argument("ICU service: null factory vector allocation");
    _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(vector, DeleteFactory_nid_no_patch, nullptr, status);
    if (*status > 0) {
        _ZN6icu_687UVectorD1Ev_nid_postfix(vector);
        Free_nid_no_patch(vector);
        return nullptr;
    }
    return vector;
}
struct LocalVector {
    Vector* vector;
    LocalVector() : vector(nullptr) {}
    ~LocalVector() {
        if (vector) {
            _ZN6icu_687UVectorD1Ev_nid_postfix(vector);
            Free_nid_no_patch(vector);
        }
    }
    void Add(UnicodeString* str, std::int32_t* status) {
        if (*status > 0) return;
        if (!vector) {
            vector = static_cast<Vector*>(Allocate_nid_no_patch(sizeof(Vector)));
            if (!vector) throw std::invalid_argument("ICU service: null cache vector allocation");
            _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(vector, DeleteUnicodeStringObj_nid_no_patch, nullptr, 5, status);
            if (*status > 0) return;
        }
        _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(vector, str, status);
    }
    std::int32_t Count() const { return vector ? vector->count : 0; }
    UnicodeString* Get(std::int32_t i) const { return static_cast<UnicodeString*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(vector, i)); }
};
struct ServiceRttiBases { TypeInfo type; std::uint32_t flags; std::uint32_t count; };
struct ServiceRttiBase { const TypeInfo* type; std::int64_t offsetFlags; };
static_assert(sizeof(ServiceRttiBases) == 24 && alignof(ServiceRttiBases) == 8);
static_assert(offsetof(ServiceRttiBases, flags) == 16 && offsetof(ServiceRttiBases, count) == 20);
static_assert(sizeof(ServiceRttiBase) == 16 && alignof(ServiceRttiBase) == 8);
static_assert(offsetof(ServiceRttiBase, type) == 0 && offsetof(ServiceRttiBase, offsetFlags) == 8);
struct ListenerSearch {
    const void* source;
    const void* target = nullptr;
    bool direct = false;
    bool sourcePublic = false;
    bool ambiguous = false;
};
void FindListener(const TypeInfo* type, const char* object, bool accessible, ListenerSearch& search) {
    Require(type); Require(type->name); Require(type->vtable);
    const auto name = std::string_view(type->name);
    if (name == "N6icu_6815ServiceListenerE") {
        if (object == search.source) search.direct = true;
        if (accessible) {
            if (search.target && search.target != object) search.ambiguous = true;
            search.target = object;
        }
    }
    if (name == "N6icu_6813EventListenerE" && object == search.source && accessible) search.sourcePublic = true;
    const auto* category = reinterpret_cast<const TypeInfo* const*>(type->vtable)[-1];
    const auto kind = std::string_view(&Require(Require(category).name));
    if (kind == "N10__cxxabiv120__si_class_type_infoE") {
        FindListener(reinterpret_cast<const DerivedTypeInfo*>(type)->base, object, accessible, search);
    } else if (kind == "N10__cxxabiv121__vmi_class_type_infoE") {
        const auto* multiple = reinterpret_cast<const ServiceRttiBases*>(type);
        const auto* bases = reinterpret_cast<const ServiceRttiBase*>(multiple + 1);
        for (std::uint32_t i = 0; i < multiple->count; ++i) {
            auto offset = bases[i].offsetFlags >> 8;
            if (bases[i].offsetFlags & 1) {
                const auto* table = *reinterpret_cast<const char* const*>(object);
                Require(table);
                offset = *reinterpret_cast<const std::ptrdiff_t*>(table + offset);
            }
            FindListener(bases[i].type, object + offset, accessible && (bases[i].offsetFlags & 2), search);
        }
    } else if (kind != "N10__cxxabiv117__class_type_infoE") {
        throw std::invalid_argument("ICU service: invalid listener RTTI");
    }
}
bool IsServiceListener(const EventListener* listener) {
    const auto* table = Require(listener).vtable;
    Require(table);
    const auto* top = reinterpret_cast<const char*>(listener) + reinterpret_cast<const std::ptrdiff_t*>(table)[-2];
    const auto* type = reinterpret_cast<const TypeInfo* const*>(table)[-1];
    ListenerSearch search{listener};
    FindListener(type, top, true, search);
    return search.direct || (search.sourcePublic && search.target && !search.ambiguous);
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
void Destroy(ICUService* self) {
    auto& object = Require(self);
    object.vtable = ServiceTable.slots.data();
    {
        std::lock_guard lock(ServiceLock);
        using Clear = void (APS5_VABI *)(ICUService*);
        Virtual_nid_no_patch<Clear>(self, 18)(self);
        if (object.factories) {
            _ZN6icu_687UVectorD1Ev_nid_postfix(object.factories);
            Free_nid_no_patch(object.factories);
            object.factories = nullptr;
        }
    }
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&object.name);
    _ZN6icu_6811ICUNotifierD2Ev_nid_postfix(reinterpret_cast<ICUNotifier*>(self));
}
}

extern "C" {
void APS5_VABI _ZN6icu_6810ICUServiceC2Ev_nid_postfix(ICUService* self) { Construct(self, nullptr); }
void APS5_VABI _ZN6icu_6810ICUServiceC2ERKNS_13UnicodeStringE_nid_postfix(ICUService* self, const UnicodeString* name) { Require(name); Construct(self, name); }
void APS5_VABI _ZN6icu_6810ICUServiceD0Ev_nid_postfix(ICUService* self) { Destroy(self); Free_nid_no_patch(self); }
void APS5_VABI _ZN6icu_6810ICUServiceD1Ev_nid_postfix(ICUService* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6810ICUServiceD2Ev_nid_postfix(ICUService* self) { Destroy(self); }
std::int8_t APS5_VABI _ZNK6icu_6810ICUService15acceptsListenerERKNS_13EventListenerE_nid_postfix(const ICUService* self, const EventListener* listener) {
    Require(self);
    Require(listener);
    return IsServiceListener(listener) ? 1 : 0;
}
void APS5_VABI _ZNK6icu_6810ICUService14notifyListenerERNS_13EventListenerE_nid_postfix(const ICUService* self, EventListener* listener) {
    Require(self);
    if (!listener) throw std::invalid_argument("ICU service: null listener");
    using ServiceChanged = void (APS5_VABI *)(const ServiceListener*, const ICUService*);
    Virtual_nid_no_patch<ServiceChanged>(reinterpret_cast<const ServiceListener*>(listener), 3)(reinterpret_cast<const ServiceListener*>(listener), self);
}
void APS5_VABI _ZN6icu_6810ICUService11clearCachesEv_nid_postfix(ICUService* self) {
    auto& object = Require(self);
    ++object.timestamp;
    ClearDNCache(object.displayNameCache);
    ClearIdCache(object.idCache);
    ClearServiceCache(object.serviceCache);
}
void APS5_VABI _ZN6icu_6810ICUService5resetEv_nid_postfix(ICUService* self) {
    Require(self);
    {
        std::lock_guard lock(ServiceLock);
        using ReInit = void (APS5_VABI *)(ICUService*);
        Virtual_nid_no_patch<ReInit>(self, 16)(self);
        using Clear = void (APS5_VABI *)(ICUService*);
        Virtual_nid_no_patch<Clear>(self, 18)(self);
    }
    Virtual_nid_no_patch<void (APS5_VABI *)(ICUService*)>(self, 4)(self);
}
const void* APS5_VABI _ZN6icu_6810ICUService15registerFactoryEPNS_17ICUServiceFactoryER10UErrorCode_nid_postfix(ICUService* self, ICUServiceFactory* factory, std::int32_t* status) {
    if (Require(status) <= 0 && factory) {
        Require(self);
        std::lock_guard lock(ServiceLock);
        if (!self->factories) {
            self->factories = CreateFactoryVector(status);
            if (*status > 0) return nullptr;
        }
        _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(self->factories, factory, 0, status);
        if (*status <= 0) {
            using Clear = void (APS5_VABI *)(ICUService*);
            Virtual_nid_no_patch<Clear>(self, 18)(self);
        } else {
            using DeleteFactory = void (APS5_VABI *)(ICUServiceFactory*);
            Virtual_nid_no_patch<DeleteFactory>(factory, 1)(factory);
            factory = nullptr;
        }
    }
    if (factory) {
        Virtual_nid_no_patch<void (APS5_VABI *)(ICUService*)>(self, 4)(self);
    }
    return factory;
}
std::int8_t APS5_VABI _ZN6icu_6810ICUService10unregisterEPKvR10UErrorCode_nid_postfix(ICUService* self, const void* key, std::int32_t* status) {
    auto* factory = const_cast<ICUServiceFactory*>(static_cast<const ICUServiceFactory*>(key));
    std::int8_t result = 0;
    if (factory && Require(self).factories) {
        std::lock_guard lock(ServiceLock);
        if (_ZN6icu_687UVector13removeElementEPv_nid_postfix(self->factories, factory)) {
            using Clear = void (APS5_VABI *)(ICUService*);
            Virtual_nid_no_patch<Clear>(self, 18)(self);
            result = 1;
        } else {
            Require(status) = 1;
            using DeleteFactory = void (APS5_VABI *)(ICUServiceFactory*);
            Virtual_nid_no_patch<DeleteFactory>(factory, 1)(factory);
        }
    }
    if (result) {
        Virtual_nid_no_patch<void (APS5_VABI *)(ICUService*)>(self, 4)(self);
    }
    return result;
}
const Hashtable* APS5_VABI _ZNK6icu_6810ICUService15getVisibleIDMapER10UErrorCode_nid_postfix(const ICUService* self, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    auto* ncthis = const_cast<ICUService*>(self);
    Require(ncthis);
    if (!ncthis->idCache) {
        ncthis->idCache = CreateHashtable(status);
        if (!ncthis->idCache) {
            *status = 7;
        } else if (ncthis->factories) {
            for (std::int32_t pos = ncthis->factories->count; --pos >= 0;) {
                auto* f = static_cast<ICUServiceFactory*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(ncthis->factories, pos));
                using UpdateVisibleIDs = void (APS5_VABI *)(const ICUServiceFactory*, Hashtable*, std::int32_t*);
                Virtual_nid_no_patch<UpdateVisibleIDs>(f, 4)(f, ncthis->idCache, status);
            }
            if (*status > 0) {
                ClearIdCache(ncthis->idCache);
            }
        }
    }
    return ncthis->idCache;
}
Vector* APS5_VABI _ZNK6icu_6810ICUService13getVisibleIDsERNS_7UVectorER10UErrorCode_nid_postfix(const ICUService* self, Vector* result, std::int32_t* status) {
    return _ZNK6icu_6810ICUService13getVisibleIDsERNS_7UVectorEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(self, result, nullptr, status);
}
Vector* APS5_VABI _ZNK6icu_6810ICUService13getVisibleIDsERNS_7UVectorEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService* self, Vector* result, const UnicodeString* matchID, std::int32_t* status) {
    Require(result);
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(result);
    if (Require(status) > 0) return result;
    {
        std::lock_guard lock(ServiceLock);
        const auto* map = _ZNK6icu_6810ICUService15getVisibleIDMapER10UErrorCode_nid_postfix(self, status);
        if (map && map->hash) {
            using CreateKey = ICUServiceKey* (APS5_VABI *)(const ICUService*, const UnicodeString*, std::int32_t*);
            LocalKey fallbackKey{Virtual_nid_no_patch<CreateKey>(self, 13)(self, matchID, status)};
            std::int32_t pos = -1;
            const Icu::UHashElement* entry = nullptr;
            while ((entry = uhash_nextElement_nid_postfix(map->hash, &pos)) != nullptr) {
                const auto* id = static_cast<const UnicodeString*>(entry->key.pointer);
                if (fallbackKey.key) {
                    using IsFallbackOf = std::int8_t (APS5_VABI *)(const ICUServiceKey*, const UnicodeString*);
                    if (!Virtual_nid_no_patch<IsFallbackOf>(fallbackKey.key, 8)(fallbackKey.key, id)) continue;
                }
                auto* idClone = CloneString(id);
                if (!idClone || (idClone->Flags() & UnicodeString::IsBogus)) {
                    if (idClone) { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(idClone); Free_nid_no_patch(idClone); }
                    *status = 7;
                    break;
                }
                _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(result, idClone, status);
                if (*status > 0) {
                    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(idClone);
                    Free_nid_no_patch(idClone);
                    break;
                }
            }
        }
    }
    if (*status > 0) {
        _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(result);
    }
    return result;
}
Object* APS5_VABI _ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService* self, ICUServiceKey* key, UnicodeString* actualReturn, std::int32_t* status) {
    return _ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyEPNS_13UnicodeStringEPKNS_17ICUServiceFactoryER10UErrorCode_nid_postfix(self, key, actualReturn, nullptr, status);
}
Object* APS5_VABI _ZNK6icu_6810ICUService6getKeyERNS_13ICUServiceKeyEPNS_13UnicodeStringEPKNS_17ICUServiceFactoryER10UErrorCode_nid_postfix(const ICUService* self, ICUServiceKey* key, UnicodeString* actualReturn, const ICUServiceFactory* factory, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    Require(self); Require(key);
    using IsDefault = std::int8_t (APS5_VABI *)(const ICUService*);
    if (Virtual_nid_no_patch<IsDefault>(self, 12)(self)) {
        using HandleDefault = Object* (APS5_VABI *)(const ICUService*, const ICUServiceKey*, UnicodeString*, std::int32_t*);
        return Virtual_nid_no_patch<HandleDefault>(self, 17)(self, key, actualReturn, status);
    }
    auto* ncthis = const_cast<ICUService*>(self);
    CacheEntry* result = nullptr;
    bool createdResult = false;
    {
        std::lock_guard lock(ServiceLock);
        if (!ncthis->serviceCache) {
            ncthis->serviceCache = CreateHashtable(status);
            if (!ncthis->serviceCache) return nullptr;
            uhash_setValueDeleter_nid_postfix(ncthis->serviceCache->hash, CacheDeleter_nid_no_patch);
        }
        LocalString currentDescriptor;
        LocalVector cacheDescriptorList;
        bool putInCache = false;
        std::int32_t startIndex = 0;
        std::int32_t limit = ncthis->factories ? ncthis->factories->count : 0;
        bool cacheResult = true;
        if (factory) {
            for (std::int32_t i = 0; i < limit; ++i) {
                if (factory == _ZNK6icu_687UVector9elementAtEi_nid_postfix(ncthis->factories, i)) {
                    startIndex = i + 1;
                    break;
                }
            }
            if (startIndex == 0) {
                *status = 1;
                return nullptr;
            }
            cacheResult = false;
        }
        using CurrentDesc = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
        using Fallback = std::int8_t (APS5_VABI *)(ICUServiceKey*);
        do {
            if (currentDescriptor.text.Flags() & UnicodeString::IsBogus) currentDescriptor.text.Empty();
            else currentDescriptor.text.SetLength(0);
            Virtual_nid_no_patch<CurrentDesc>(key, 6)(key, &currentDescriptor.text);
            result = static_cast<CacheEntry*>(uhash_get_nid_postfix(ncthis->serviceCache->hash, &currentDescriptor.text));
            if (result) break;
            putInCache = true;
            std::int32_t index = startIndex;
            while (index < limit) {
                auto* f = static_cast<ICUServiceFactory*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(ncthis->factories, index++));
                using Create = Object* (APS5_VABI *)(const ICUServiceFactory*, const ICUServiceKey*, const ICUService*, std::int32_t*);
                auto* service = Virtual_nid_no_patch<Create>(f, 3)(f, key, self, status);
                if (*status > 0) { DeleteObject(service); return nullptr; }
                if (service) {
                    result = static_cast<CacheEntry*>(Allocate_nid_no_patch(sizeof(CacheEntry)));
                    if (!result) { DeleteObject(service); *status = 7; return nullptr; }
                    new (result) CacheEntry(currentDescriptor.text, service);
                    createdResult = true;
                    goto outerEnd;
                }
            }
            auto* idToCache = CloneString(&currentDescriptor.text);
            if (!idToCache || (idToCache->Flags() & UnicodeString::IsBogus)) {
                if (idToCache) { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(idToCache); Free_nid_no_patch(idToCache); }
                *status = 7;
                return nullptr;
            }
            cacheDescriptorList.Add(idToCache, status);
            if (*status > 0) return nullptr;
        } while (Virtual_nid_no_patch<Fallback>(key, 7)(key));
    outerEnd:
        if (result) {
            if (putInCache && cacheResult) {
                uhash_put_nid_postfix(ncthis->serviceCache->hash, CloneString(&result->actualDescriptor), result, status);
                if (*status > 0) return nullptr;
                for (std::int32_t i = cacheDescriptorList.Count(); --i >= 0;) {
                    auto* desc = cacheDescriptorList.Get(i);
                    uhash_put_nid_postfix(ncthis->serviceCache->hash, CloneString(desc), result, status);
                    if (*status > 0) return nullptr;
                    result->Ref();
                }
            }
            if (actualReturn) {
                if (result->actualDescriptor.Length() > 0 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result->actualDescriptor, 0) == u'/') {
                    _ZNK6icu_6813UnicodeString14extractBetweenEiiRS0__nid_postfix(&result->actualDescriptor, 1, result->actualDescriptor.Length(), actualReturn);
                } else {
                    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(actualReturn, &result->actualDescriptor);
                }
                if (actualReturn->Flags() & UnicodeString::IsBogus) {
                    *status = 7;
                    if (putInCache && !cacheResult) result->Unref();
                    return nullptr;
                }
            }
            if (putInCache && !cacheResult && !createdResult) throw std::invalid_argument("ICU service: deleting a shared cache result");
            using CloneInstance = Object* (APS5_VABI *)(const ICUService*, Object*);
            auto* service = Virtual_nid_no_patch<CloneInstance>(self, 14)(self, result->service);
            if (putInCache && !cacheResult) result->Unref();
            return service;
        }
    }
    using HandleDefault = Object* (APS5_VABI *)(const ICUService*, const ICUServiceKey*, UnicodeString*, std::int32_t*);
    return Virtual_nid_no_patch<HandleDefault>(self, 17)(self, key, actualReturn, status);
}

UnicodeString* APS5_VABI _ZNK6icu_6810ICUService14getDisplayNameERKNS_13UnicodeStringERS1_RKNS_6LocaleE_nid_postfix(const ICUService* self, const UnicodeString* id, UnicodeString* result, const Locale* locale) {
    Require(self); Require(id); Require(result); Require(locale);
    std::int32_t status = 0;
    std::lock_guard lock(ServiceLock);
    const auto* map = _ZNK6icu_6810ICUService15getVisibleIDMapER10UErrorCode_nid_postfix(self, &status);
    if (map && map->hash) {
        auto* f = static_cast<ICUServiceFactory*>(uhash_get_nid_postfix(map->hash, id));
        if (f) {
            using GetDisplayName = UnicodeString* (APS5_VABI *)(const ICUServiceFactory*, const UnicodeString*, const Locale*, UnicodeString*);
            Virtual_nid_no_patch<GetDisplayName>(f, 5)(f, id, locale, result);
            return result;
        }
        status = 0;
        using CreateKey = ICUServiceKey* (APS5_VABI *)(const ICUService*, const UnicodeString*, std::int32_t*);
        LocalKey fallbackKey{Virtual_nid_no_patch<CreateKey>(self, 13)(self, id, &status)};
        using Fallback = std::int8_t (APS5_VABI *)(ICUServiceKey*);
        using CurrentId = UnicodeString* (APS5_VABI *)(const ICUServiceKey*, UnicodeString*);
        while (fallbackKey.key && Virtual_nid_no_patch<Fallback>(fallbackKey.key, 7)(fallbackKey.key)) {
            LocalString us;
            Virtual_nid_no_patch<CurrentId>(fallbackKey.key, 5)(fallbackKey.key, &us.text);
            f = static_cast<ICUServiceFactory*>(uhash_get_nid_postfix(map->hash, &us.text));
            if (f) {
                using GetDisplayName = UnicodeString* (APS5_VABI *)(const ICUServiceFactory*, const UnicodeString*, const Locale*, UnicodeString*);
                Virtual_nid_no_patch<GetDisplayName>(f, 5)(f, id, locale, result);
                return result;
            }
        }
    }
    _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(result);
    return result;
}

Vector* APS5_VABI _ZNK6icu_6810ICUService15getDisplayNamesERNS_7UVectorERKNS_6LocaleER10UErrorCode_nid_postfix(const ICUService* self, Vector* result, const Locale* locale, std::int32_t* status) {
    return _ZNK6icu_6810ICUService15getDisplayNamesERNS_7UVectorERKNS_6LocaleEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(self, result, locale, nullptr, status);
}
Vector* APS5_VABI _ZNK6icu_6810ICUService15getDisplayNamesERNS_7UVectorERKNS_6LocaleEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(const ICUService* self, Vector* result, const Locale* locale, const UnicodeString* matchID, std::int32_t* status) {
    Require(result); Require(locale);
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(result);
    _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(result, reinterpret_cast<Icu::ObjectDeleter>(DeleteStringPair_nid_no_patch));
    Require(status);
    auto* ncthis = const_cast<ICUService*>(self);
    Require(ncthis);
    std::lock_guard lock(ServiceLock);
    if (*status <= 0 && ncthis->displayNameCache) {
        auto* dn = static_cast<DNCache*>(ncthis->displayNameCache);
        if (!_ZNK6icu_686LocaleeqERKS0__nid_postfix(&dn->locale, locale)) {
            ClearDNCache(ncthis->displayNameCache);
        }
    }
    if (*status <= 0 && !ncthis->displayNameCache) {
        const auto* m = _ZNK6icu_6810ICUService15getVisibleIDMapER10UErrorCode_nid_postfix(self, status);
        if (*status > 0) return result;
        auto* dn = static_cast<DNCache*>(Allocate_nid_no_patch(sizeof(DNCache)));
        if (!dn) { *status = 7; return result; }
        _ZN6icu_686LocaleC1ERKS0__nid_postfix(&dn->locale, locale);
        std::int32_t cacheStatus = 0;
        uhash_init_nid_postfix(&dn->cache.hashObject, reinterpret_cast<Icu::HashFunction>(uhash_hashUnicodeString_nid_postfix), reinterpret_cast<Icu::HashComparator>(uhash_compareUnicodeString_nid_postfix), nullptr, &cacheStatus);
        dn->cache.hash = cacheStatus <= 0 ? &dn->cache.hashObject : nullptr;
        if (dn->cache.hash) uhash_setKeyDeleter_nid_postfix(dn->cache.hash, DeleteUnicodeStringObj_nid_no_patch);
        ncthis->displayNameCache = dn;
        std::int32_t pos = -1;
        const Icu::UHashElement* entry = nullptr;
        while ((entry = uhash_nextElement_nid_postfix(m->hash, &pos)) != nullptr) {
            const auto* id = static_cast<const UnicodeString*>(entry->key.pointer);
            auto* f = static_cast<ICUServiceFactory*>(entry->value.pointer);
            LocalString dname;
            using GetDisplayName = UnicodeString* (APS5_VABI *)(const ICUServiceFactory*, const UnicodeString*, const Locale*, UnicodeString*);
            Virtual_nid_no_patch<GetDisplayName>(f, 5)(f, id, locale, &dname.text);
            if (dname.text.Flags() & UnicodeString::IsBogus) {
                *status = 7;
            } else {
                auto* keyCopy = CloneString(&dname.text);
                uhash_put_nid_postfix(dn->cache.hash, keyCopy, const_cast<UnicodeString*>(id), status);
                if (*status <= 0) continue;
            }
            ClearDNCache(ncthis->displayNameCache);
            return result;
        }
    }
    auto* dn = static_cast<DNCache*>(ncthis->displayNameCache);
    Require(dn);
    using CreateKey = ICUServiceKey* (APS5_VABI *)(const ICUService*, const UnicodeString*, std::int32_t*);
    LocalKey matchKey{Virtual_nid_no_patch<CreateKey>(self, 13)(self, matchID, status)};
    std::int32_t pos = -1;
    const Icu::UHashElement* entry = nullptr;
    while ((entry = uhash_nextElement_nid_postfix(dn->cache.hash, &pos)) != nullptr) {
        const auto* id = static_cast<const UnicodeString*>(entry->value.pointer);
        if (matchKey.key) {
            using IsFallbackOf = std::int8_t (APS5_VABI *)(const ICUServiceKey*, const UnicodeString*);
            if (!Virtual_nid_no_patch<IsFallbackOf>(matchKey.key, 8)(matchKey.key, id)) continue;
        }
        const auto* dname = static_cast<const UnicodeString*>(entry->key.pointer);
        auto* pair = _ZN6icu_6810StringPair6createERKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(id, dname, status);
        if (*status > 0) { _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(result); break; }
        _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(result, pair, status);
        if (*status > 0) {
            DeleteStringPair_nid_no_patch(pair);
            _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(result);
            break;
        }
    }
    return result;
}
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
