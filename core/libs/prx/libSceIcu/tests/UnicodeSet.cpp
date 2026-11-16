// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    Icu::Abi::UnicodeSet set{};
    _ZN6icu_6810UnicodeSetC1Ev_nid_postfix(&set);
    Check(_ZNK6icu_6810UnicodeSet7isEmptyEv_nid_postfix(&set));
    _ZN6icu_6810UnicodeSet3addEii_nid_postfix(&set, 3, 7);
    _ZN6icu_6810UnicodeSet3addEi_nid_postfix(&set, 8);
    Check(_ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(&set) == 6);
    Check(_ZNK6icu_6810UnicodeSet13getRangeCountEv_nid_postfix(&set) == 1);
    Check(_ZNK6icu_6810UnicodeSet13getRangeStartEi_nid_postfix(&set, 0) == 3);
    Check(_ZNK6icu_6810UnicodeSet11getRangeEndEi_nid_postfix(&set, 0) == 8);
    _ZN6icu_6810UnicodeSet6removeEii_nid_postfix(&set, 4, 6);
    Check(_ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(&set) == 3);
    Check(_ZNK6icu_6810UnicodeSet6charAtEi_nid_postfix(&set, 1) == 7);
    Check(_ZNK6icu_6810UnicodeSet7indexOfEi_nid_postfix(&set, 8) == 2);
    _ZN6icu_6810UnicodeSet10complementEv_nid_postfix(&set);
    Check(_ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(&set) == 0x110000 - 3);
    _ZN6icu_6810UnicodeSet6retainEii_nid_postfix(&set, 0, 8);
    Check(_ZNK6icu_6810UnicodeSet4sizeEv_nid_postfix(&set) == 6);
    Icu::Abi::UnicodeString text{};
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&text, u"abc");
    _ZN6icu_6810UnicodeSet3addERKNS_13UnicodeStringE_nid_postfix(&set, &text);
    Check(_ZNK6icu_6810UnicodeSet8containsERKNS_13UnicodeStringE_nid_postfix(&set, &text));
    Check(_ZNK6icu_6810UnicodeSet11stringsSizeEv_nid_postfix(&set) == 1);
    _ZN6icu_6810UnicodeSet10complementERKNS_13UnicodeStringE_nid_postfix(&set, &text);
    Check(!_ZNK6icu_6810UnicodeSet10hasStringsEv_nid_postfix(&set));
    _ZN6icu_6810UnicodeSet3addEi_nid_postfix(&set, std::numeric_limits<std::int32_t>::max());
    Check(_ZNK6icu_6810UnicodeSet8containsEi_nid_postfix(&set, 0x10ffff));
    _ZN6icu_6810UnicodeSet10setToBogusEv_nid_postfix(&set);
    Check(set.fFlags == 1 && set.len == 1);
    _ZN6icu_6810UnicodeSet3addEi_nid_postfix(&set, 1);
    Check(_ZNK6icu_6810UnicodeSet7isEmptyEv_nid_postfix(&set));
    _ZN6icu_6810UnicodeSet5clearEv_nid_postfix(&set);
    Check(set.fFlags == 0);
    _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(&set);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&text);
    try { _ZN6icu_6810UnicodeSetC1Ev_nid_postfix(nullptr); std::abort(); }
    catch (const std::invalid_argument&) {}
    std::puts("ICU UnicodeSet tests passed");
}
