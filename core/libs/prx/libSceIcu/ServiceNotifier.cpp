// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <mutex>
#include <new>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU notifier: null pointer");
    return *value;
}
std::mutex NotifyLock;
std::array<char, 2> ListenerIds{};
template<std::size_t TCount> struct Table {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, TCount> slots;
};
static_assert(sizeof(Table<7>) == 72 && alignof(Table<7>) == 8 && offsetof(Table<7>, slots) == 16);
const TypeInfo MemoryType{ClassTypeTable_nid_no_patch(), "N6icu_687UMemoryE"};
const DerivedTypeInfo NotifierType{{SingleTypeTable_nid_no_patch(), "N6icu_6811ICUNotifierE"}, &MemoryType};
const DerivedTypeInfo EventType{{SingleTypeTable_nid_no_patch(), "N6icu_6813EventListenerE"}, Rtti_nid_no_patch(Class::Object)};
const DerivedTypeInfo ListenerType{{SingleTypeTable_nid_no_patch(), "N6icu_6815ServiceListenerE"}, &EventType.type};
const DerivedTypeInfo VectorType{{SingleTypeTable_nid_no_patch(), "N6icu_687UVectorE"}, Rtti_nid_no_patch(Class::Object)};
void APS5_VABI Abstract_nid_no_patch() { throw std::invalid_argument("ICU notifier: abstract virtual call"); }
void APS5_VABI DestroyVector_nid_no_patch(Vector* vector) {
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(vector);
    Free_nid_no_patch(vector->elements);
}
void APS5_VABI DeleteVector_nid_no_patch(Vector* vector) { DestroyVector_nid_no_patch(vector); Free_nid_no_patch(vector); }
const Table<3> VectorTable{0, &VectorType.type, {{
    reinterpret_cast<GuestFunction>(DestroyVector_nid_no_patch),
    reinterpret_cast<GuestFunction>(DeleteVector_nid_no_patch),
    reinterpret_cast<GuestFunction>(_ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix)
}}};
const Table<7> NotifierTable{0, &NotifierType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifierD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifierD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix),
    Abstract_nid_no_patch, Abstract_nid_no_patch
}}};
const Table<3> EventTable{0, &EventType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6813EventListenerD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6813EventListenerD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix)
}}};
const Table<4> ListenerTable{0, &ListenerType.type, {{
    reinterpret_cast<GuestFunction>(_ZN6icu_6815ServiceListenerD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6815ServiceListenerD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix),
    Abstract_nid_no_patch
}}};
void Destroy(ICUNotifier* self) {
    auto& object = Require(self);
    object.vtable = NotifierTable.slots.data();
    std::lock_guard lock(NotifyLock);
    if (object.listeners) DeleteVector_nid_no_patch(object.listeners);
    object.listeners = nullptr;
}
}

extern "C" {
void APS5_VABI _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(ICUNotifier* self) {
    auto& object = Require(self);
    object.vtable = NotifierTable.slots.data();
    object.listeners = nullptr;
}
void APS5_VABI _ZN6icu_6811ICUNotifierD1Ev_nid_postfix(ICUNotifier* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6811ICUNotifierD2Ev_nid_postfix(ICUNotifier* self) { Destroy(self); }
void APS5_VABI _ZN6icu_6811ICUNotifierD0Ev_nid_postfix(ICUNotifier* self) { Destroy(self); Free_nid_no_patch(self); }
void APS5_VABI _ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(ICUNotifier* self, const EventListener* listener, std::int32_t* status) {
    if (Require(status) > 0) return;
    if (!listener) { *status = 1; return; }
    Require(self);
    using Accept = std::int8_t (APS5_VABI *)(const ICUNotifier*, const EventListener*);
    if (!Virtual_nid_no_patch<Accept>(self, 5)(self, listener)) return;
    std::lock_guard lock(NotifyLock);
    if (!self->listeners) {
        auto* vector = static_cast<Vector*>(Allocate_nid_no_patch(sizeof(Vector)));
        if (!vector) throw std::invalid_argument("ICU notifier: null vector allocation dereference");
        new (vector) Vector{};
        vector->vtable = VectorTable.slots.data();
        _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(vector, 5, status);
        self->listeners = vector;
    } else {
        for (std::int32_t i = 0; i < self->listeners->count; ++i) {
            if (_ZNK6icu_687UVector9elementAtEi_nid_postfix(self->listeners, i) == listener) return;
        }
    }
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(self->listeners, const_cast<EventListener*>(listener), status);
}
void APS5_VABI _ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(ICUNotifier* self, const EventListener* listener, std::int32_t* status) {
    if (Require(status) > 0) return;
    if (!listener) { *status = 1; return; }
    auto& object = Require(self);
    std::lock_guard lock(NotifyLock);
    if (!object.listeners) return;
    for (std::int32_t i = 0; i < object.listeners->count; ++i) {
        if (_ZNK6icu_687UVector9elementAtEi_nid_postfix(object.listeners, i) != listener) continue;
        _ZN6icu_687UVector15removeElementAtEi_nid_postfix(object.listeners, i);
        if (!object.listeners->count) { DeleteVector_nid_no_patch(object.listeners); object.listeners = nullptr; }
        return;
    }
}
void APS5_VABI _ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix(ICUNotifier* self) {
    auto& object = Require(self);
    if (!object.listeners) return;
    std::lock_guard lock(NotifyLock);
    if (!object.listeners) return;
    using Notify = void (APS5_VABI *)(const ICUNotifier*, EventListener*);
    for (std::int32_t i = 0, end = object.listeners->count; i < end; ++i) {
        auto* listener = static_cast<EventListener*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(object.listeners, i));
        if (!listener) throw std::invalid_argument("ICU notifier: null listener dereference");
        Virtual_nid_no_patch<Notify>(self, 6)(self, listener);
    }
}
void APS5_VABI _ZN6icu_6813EventListenerD1Ev_nid_postfix(EventListener* self) {
    Require(self).vtable = EventTable.slots.data();
}
void APS5_VABI _ZN6icu_6813EventListenerD2Ev_nid_postfix(EventListener* self) {
    Require(self).vtable = EventTable.slots.data();
}
void APS5_VABI _ZN6icu_6813EventListenerD0Ev_nid_postfix(EventListener* self) {
    Require(self).vtable = EventTable.slots.data();
    Free_nid_no_patch(self);
}
void* APS5_VABI _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix() { return &ListenerIds[0]; }
void* APS5_VABI _ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(const EventListener*) { return &ListenerIds[0]; }
void APS5_VABI _ZN6icu_6815ServiceListenerD1Ev_nid_postfix(ServiceListener* self) {
    Require(self).vtable = ListenerTable.slots.data();
    _ZN6icu_6813EventListenerD2Ev_nid_postfix(reinterpret_cast<EventListener*>(self));
}
void APS5_VABI _ZN6icu_6815ServiceListenerD2Ev_nid_postfix(ServiceListener* self) {
    Require(self).vtable = ListenerTable.slots.data();
    _ZN6icu_6813EventListenerD2Ev_nid_postfix(reinterpret_cast<EventListener*>(self));
}
void APS5_VABI _ZN6icu_6815ServiceListenerD0Ev_nid_postfix(ServiceListener* self) {
    Require(self).vtable = ListenerTable.slots.data();
    _ZN6icu_6813EventListenerD2Ev_nid_postfix(reinterpret_cast<EventListener*>(self));
    Free_nid_no_patch(self);
}
void* APS5_VABI _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix() { return &ListenerIds[1]; }
void* APS5_VABI _ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(const ServiceListener*) { return &ListenerIds[1]; }
}
