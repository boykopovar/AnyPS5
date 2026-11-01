// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <string>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    UnicodeString id{}, fallback{}, out{}; out.Init();
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&id, u"EN_us_POSIX", 11);
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&fallback, u"fr_CA", 5);
    std::int32_t status = -1;
    auto* key = _ZN6icu_689LocaleKey27createWithCanonicalFallbackEPKNS_13UnicodeStringES3_iR10UErrorCode_nid_postfix(&id, &fallback, 2, &status);
    Check(key && status == -1);
    Check(std::u16string(key->currentId.Buffer(), key->currentId.Length()) == u"en_US_POSIX");
    for (auto expected : {u"en_US", u"en", u"fr_CA", u"fr", u""}) {
        Check(_ZN6icu_689LocaleKey8fallbackEv_nid_postfix(key));
        Check(std::u16string(key->currentId.Buffer(), key->currentId.Length()) == expected);
    }
    Check(!_ZN6icu_689LocaleKey8fallbackEv_nid_postfix(key));
    _ZNK6icu_689LocaleKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(key, &out);
    Check(out.Flags() & UnicodeString::IsBogus);
    _ZN6icu_689LocaleKeyD0Ev_nid_postfix(key);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&fallback); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&out);
}
