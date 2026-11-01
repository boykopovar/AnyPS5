// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

namespace {
using namespace Icu::Abi;
int Calls;
std::int8_t APS5_VABI Accept(const ICUNotifier*, const EventListener*) { return 1; }
void APS5_VABI Notify(const ICUNotifier*, EventListener*) { ++Calls; }
}
int main() {
    ICUNotifier notifier{};
    _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(&notifier);
    Check(notifier.listeners == nullptr);
    std::array<GuestFunction, 7> table{};
    std::copy_n(notifier.vtable, 7, table.begin());
    table[5] = reinterpret_cast<GuestFunction>(Accept);
    table[6] = reinterpret_cast<GuestFunction>(Notify);
    notifier.vtable = table.data();
    EventListener listeners[2]{};
    std::int32_t status = 0;
    _ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(&notifier, nullptr, &status);
    Check(status == 1 && notifier.listeners == nullptr);
    status = -1;
    for (auto* listener : {listeners, listeners + 1, listeners}) {
        _ZN6icu_6811ICUNotifier11addListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(&notifier, listener, &status);
    }
    Check(status == -1 && notifier.listeners->count == 2 && notifier.listeners->capacity == 5);
    _ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix(&notifier);
    Check(Calls == 2);
    _ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(&notifier, listeners, &status);
    _ZN6icu_6811ICUNotifier13notifyChangedEv_nid_postfix(&notifier);
    Check(Calls == 3);
    _ZN6icu_6811ICUNotifier14removeListenerEPKNS_13EventListenerER10UErrorCode_nid_postfix(&notifier, listeners + 1, &status);
    Check(notifier.listeners == nullptr);
    _ZN6icu_6811ICUNotifierD2Ev_nid_postfix(&notifier);
    Check(_ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix() != _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix());
    Check(_ZNK6icu_6813EventListener17getDynamicClassIDEv_nid_postfix(nullptr) == _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix());
    Check(_ZNK6icu_6815ServiceListener17getDynamicClassIDEv_nid_postfix(nullptr) == _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix());
    bool caught = false;
    try { _ZN6icu_6811ICUNotifierC2Ev_nid_postfix(nullptr); } catch (const std::invalid_argument&) { caught = true; }
    Check(caught);
}
