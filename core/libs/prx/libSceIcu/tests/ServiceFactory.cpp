// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    using namespace Icu::Abi;
    UnicodeString id{}, output{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&id, u"test", 4);
    output.Init();
    SimpleFactory factory{};
    _ZN6icu_6813SimpleFactoryC1EPNS_7UObjectERKNS_13UnicodeStringEa_nid_postfix(&factory, nullptr, &id, -7);
    Check(factory.visible == -7 && factory.instance == nullptr);
    Check(_ZNK6icu_6813SimpleFactory14getDisplayNameERKNS_13UnicodeStringERKNS_6LocaleERS1__nid_postfix(&factory, &id, nullptr, &output) == &output);
    Check(_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&id, &output));
    std::int32_t status = 5;
    Check(!_ZNK6icu_6813SimpleFactory6createERKNS_13ICUServiceKeyEPKNS_10ICUServiceER10UErrorCode_nid_postfix(nullptr, nullptr, nullptr, &status));
    Check(status == 5);
    _ZN6icu_6813SimpleFactoryD2Ev_nid_postfix(&factory);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&output);
}
