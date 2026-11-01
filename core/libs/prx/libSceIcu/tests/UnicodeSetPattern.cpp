// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
void Text(const Icu::Abi::UnicodeString& text,const char16_t* expected,std::int32_t length) {
    Check(text.Length()==length && !std::memcmp(text.Buffer(),expected,length*2));
}
}
int main() {
    Icu::Abi::UnicodeSet set{};
    Icu::Abi::UnicodeString output{};
    _ZN6icu_6810UnicodeSetC1Eii_nid_postfix(&set,'a','z');
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&output,u"");
    _ZNK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix(&set,&output,1);
    Text(output,u"[a-z]",5);
    _ZN6icu_6810UnicodeSet12_appendToPatERNS_13UnicodeStringEia_nid_postfix(&output,0x1f600,1);
    Text(output,u"[a-z]\\U0001F600",15);
    _ZN6icu_6810UnicodeSet10setPatternEPKDsi_nid_postfix(&set,u"[\\\u0001]",4);
    _ZNK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix(&set,&output,1);
    Text(output,u"[\\u0001]",8);
    Icu::Abi::UnicodeString property{};
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&property,u"\\p{L}");
    Check(_ZN6icu_6810UnicodeSet24resemblesPropertyPatternERKNS_13UnicodeStringEi_nid_postfix(&property,0));
    Check(_ZN6icu_6810UnicodeSet16resemblesPatternERKNS_13UnicodeStringEi_nid_postfix(&property,0));
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&property);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&output);
    _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(&set);
    std::puts("ICU UnicodeSetPattern tests passed");
}
