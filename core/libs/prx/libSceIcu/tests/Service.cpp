// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    ICUService service{};
    _ZN6icu_6810ICUServiceC2Ev_nid_postfix(&service);
    Check(_ZNK6icu_6810ICUService9isDefaultEv_nid_postfix(&service));
    Check(_ZNK6icu_6810ICUService12getTimestampEv_nid_postfix(&service) == 0);
    UnicodeString id{}; _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&id, u"test", 4);
    std::int32_t status = -1;
    auto* key = _ZNK6icu_6810ICUService9createKeyEPKNS_13UnicodeStringER10UErrorCode_nid_postfix(&service, &id, &status);
    Check(key && status == -1);
    Check(_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&key->id, &id));
    _ZN6icu_6813ICUServiceKeyD0Ev_nid_postfix(key);
    Check(!_ZNK6icu_6810ICUService13handleDefaultERKNS_13ICUServiceKeyEPNS_13UnicodeStringER10UErrorCode_nid_postfix(nullptr, nullptr, nullptr, nullptr));
    auto* factory = static_cast<SimpleFactory*>(Allocate_nid_no_patch(sizeof(SimpleFactory)));
    _ZN6icu_6813SimpleFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(factory, nullptr, &id, 1);
    auto* registration = _ZN6icu_6810ICUService15registerFactoryEPNS_17ICUServiceFactoryER10UErrorCode_nid_postfix(&service, reinterpret_cast<ICUServiceFactory*>(factory), &status);
    Check(registration == factory && status == -1);
    Check(_ZNK6icu_6810ICUService14countFactoriesEv_nid_postfix(&service) == 1);
    Check(_ZNK6icu_6810ICUService12getTimestampEv_nid_postfix(&service) == 1);
    Vector visible{};
    _ZN6icu_687UVectorC1ER10UErrorCode_nid_postfix(&visible, &status);
    _ZNK6icu_6810ICUService13getVisibleIDsERNS_7UVectorER10UErrorCode_nid_postfix(&service, &visible, &status);
    Check(visible.count == 1 && status == -1);
    auto* copy = static_cast<UnicodeString*>(visible.elements[0].pointer);
    Check(_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(copy, &id));
    _ZN6icu_6813UnicodeStringD0Ev_nid_postfix(copy);
    _ZN6icu_687UVectorD1Ev_nid_postfix(&visible);
    Check(_ZN6icu_6810ICUService10unregisterEPKvR10UErrorCode_nid_postfix(&service, registration, &status));
    Check(_ZNK6icu_6810ICUService9isDefaultEv_nid_postfix(&service));
    _ZN6icu_6810ICUService5resetEv_nid_postfix(&service);
    Check(_ZNK6icu_6810ICUService12getTimestampEv_nid_postfix(&service) == 3);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id);
    _ZN6icu_6810ICUServiceD1Ev_nid_postfix(&service);
    Check(!service.factories && !service.listeners);
}
