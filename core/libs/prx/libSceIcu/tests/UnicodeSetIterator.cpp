// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>

namespace { void Check(bool value) { if(!value)std::abort(); } }
int main() {
    Icu::Abi::UnicodeSet set{};_ZN6icu_6810UnicodeSetC1Eii_nid_postfix(&set,3,5);
    Icu::Abi::UnicodeString text{};_ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&text,u"abc");
    _ZN6icu_6810UnicodeSet3addERKNS_13UnicodeStringE_nid_postfix(&set,&text);
    Icu::Abi::UnicodeSetIterator iterator{};
    _ZN6icu_6818UnicodeSetIteratorC1ERKNS_10UnicodeSetE_nid_postfix(&iterator,&set);
    Check(_ZN6icu_6818UnicodeSetIterator9nextRangeEv_nid_postfix(&iterator));
    Check(iterator.codepoint==3 && iterator.codepointEnd==5);
    const auto* point=_ZN6icu_6818UnicodeSetIterator9getStringEv_nid_postfix(&iterator);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(point)==1);
    Check(_ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(point,0)==3);
    Check(_ZN6icu_6818UnicodeSetIterator4nextEv_nid_postfix(&iterator) && iterator.codepoint==-1);
    const auto* string=_ZN6icu_6818UnicodeSetIterator9getStringEv_nid_postfix(&iterator);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(string)==3);
    Check(!_ZN6icu_6818UnicodeSetIterator4nextEv_nid_postfix(&iterator));
    _ZN6icu_6818UnicodeSetIterator5resetEv_nid_postfix(&iterator);
    for(int i=3;i<=5;++i)Check(_ZN6icu_6818UnicodeSetIterator4nextEv_nid_postfix(&iterator) && iterator.codepoint==i);
    _ZN6icu_6818UnicodeSetIteratorD1Ev_nid_postfix(&iterator);
    _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(&set);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text);
    std::puts("ICU UnicodeSetIterator tests passed");
}
