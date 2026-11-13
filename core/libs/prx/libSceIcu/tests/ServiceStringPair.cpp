// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    UnicodeString id{}; _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&id, u"a", 1);
    std::int32_t status = -1;
    auto* pair = _ZN6icu_6810StringPair6createERKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(&id, &id, &status);
    Check(pair && status == -1 && !_ZNK6icu_6810StringPair7isBogusEv_nid_postfix(pair));
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->id); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->displayName); _ZN6icu_687UMemorydlEPv_nid_postfix(pair);
    _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(&id);
    Check(!_ZN6icu_6810StringPair6createERKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(&id, &id, &status) && status == 7);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id);
}
