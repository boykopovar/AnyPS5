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
    UnicodeString id{}, out{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&id, u"abc", 3);
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&out, u"pre", 3);
    ICUServiceKey key{};
    _ZN6icu_6813ICUServiceKeyC1ERKNS_13UnicodeStringE_nid_postfix(&key, &id);
    Check(_ZNK6icu_6813ICUServiceKey5getIDEv_nid_postfix(&key) == &key.id);
    Check(_ZNK6icu_6813ICUServiceKey17currentDescriptorERNS_13UnicodeStringE_nid_postfix(&key, &out) == &out);
    Check(std::u16string(out.Buffer(), out.Length()) == u"pre/abc");
    _ZN6icu_6813ICUServiceKey11parsePrefixERNS_13UnicodeStringE_nid_postfix(&out);
    Check(std::u16string(out.Buffer(), out.Length()) == u"pre");
    _ZN6icu_6813ICUServiceKey11parseSuffixERNS_13UnicodeStringE_nid_postfix(&out);
    Check(out.Length() == 3);
    _ZN6icu_6813ICUServiceKey11parsePrefixERNS_13UnicodeStringE_nid_postfix(&out);
    Check(out.Length() == 0);
    Check(_ZNK6icu_6813ICUServiceKey12isFallbackOfERKNS_13UnicodeStringE_nid_postfix(&key, &id));
    Check(!_ZN6icu_6813ICUServiceKey8fallbackEv_nid_postfix(&key));
    _ZN6icu_6813ICUServiceKeyD1Ev_nid_postfix(&key);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&id);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&out);
}
