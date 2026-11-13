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
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&service.name);
    _ZN6icu_6811ICUNotifierD2Ev_nid_postfix(reinterpret_cast<ICUNotifier*>(&service));
}
