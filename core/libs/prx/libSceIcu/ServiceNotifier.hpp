// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include "Vector.hpp"

namespace Icu::Abi {
struct EventListener { const GuestFunction* vtable; };
struct ServiceListener { const GuestFunction* vtable; };
struct ICUNotifier { const GuestFunction* vtable; Vector* listeners; };
struct NotifierFunctions {
    void (APS5_VABI *destroy)(ICUNotifier*);
    void (APS5_VABI *deleteObject)(ICUNotifier*);
    void (APS5_VABI *addListener)(ICUNotifier*, const EventListener*, std::int32_t*);
    void (APS5_VABI *removeListener)(ICUNotifier*, const EventListener*, std::int32_t*);
    void (APS5_VABI *notifyChanged)(ICUNotifier*);
    std::int8_t (APS5_VABI *acceptsListener)(const ICUNotifier*, const EventListener*);
    void (APS5_VABI *notifyListener)(const ICUNotifier*, EventListener*);
};
static_assert(sizeof(EventListener) == 8 && alignof(EventListener) == 8 && offsetof(EventListener, vtable) == 0);
static_assert(sizeof(ServiceListener) == 8 && alignof(ServiceListener) == 8 && offsetof(ServiceListener, vtable) == 0);
static_assert(sizeof(ICUNotifier) == 16 && alignof(ICUNotifier) == 8);
static_assert(offsetof(ICUNotifier, vtable) == 0 && offsetof(ICUNotifier, listeners) == 8);
static_assert(sizeof(NotifierFunctions) == 56 && alignof(NotifierFunctions) == 8);
static_assert(offsetof(NotifierFunctions, addListener) == 16 && offsetof(NotifierFunctions, notifyListener) == 48);
}

extern "C" {
void APS5_VABI _ZN6icu_6813EventListenerD0Ev_nid_postfix(Icu::Abi::EventListener* self);
void APS5_VABI _ZN6icu_6813EventListenerD1Ev_nid_postfix(Icu::Abi::EventListener* self);
void APS5_VABI _ZN6icu_6813EventListenerD2Ev_nid_postfix(Icu::Abi::EventListener* self);
void* APS5_VABI _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(const Icu::Abi::EventListener* self);
void APS5_VABI _ZN6icu_6815ServiceListenerD0Ev_nid_postfix(Icu::Abi::ServiceListener* self);
void APS5_VABI _ZN6icu_6815ServiceListenerD1Ev_nid_postfix(Icu::Abi::ServiceListener* self);
void APS5_VABI _ZN6icu_6815ServiceListenerD2Ev_nid_postfix(Icu::Abi::ServiceListener* self);
void* APS5_VABI _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(const Icu::Abi::ServiceListener* self);
void APS5_VABI _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(Icu::Abi::ICUNotifier* self);
void APS5_VABI _ZN6icu_6811ICUNotifierD0Ev_nid_postfix(Icu::Abi::ICUNotifier* self);
void APS5_VABI _ZN6icu_6811ICUNotifierD1Ev_nid_postfix(Icu::Abi::ICUNotifier* self);
void APS5_VABI _ZN6icu_6811ICUNotifierD2Ev_nid_postfix(Icu::Abi::ICUNotifier* self);
void APS5_VABI _ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(Icu::Abi::ICUNotifier* self, const Icu::Abi::EventListener* listener, std::int32_t* status);
void APS5_VABI _ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(Icu::Abi::ICUNotifier* self, const Icu::Abi::EventListener* listener, std::int32_t* status);
void APS5_VABI _ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix(Icu::Abi::ICUNotifier* self);
}
