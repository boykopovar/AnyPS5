// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

void Check(bool value) { if (!value) std::abort(); }
int main() {
    using namespace Icu::Abi;
    UnicodeString first{}, second{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&first, u"abc", 3);
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&second, u"abc", 3);
    Element a{&first}, b{&second}, empty{nullptr};
    Check(uhash_hashUnicodeString_nid_postfix(empty) == 0);
    Check(uhash_hashUnicodeString_nid_postfix(a) == 136518);
    Check(uhash_compareUnicodeString_nid_postfix(a, b) == 1);
    Check(uhash_compareUnicodeString_nid_postfix(empty, empty) == 1);
    Check(uhash_compareUnicodeString_nid_postfix(a, empty) == 0);
    _ZN6icu_6813UnicodeString5setToEDs_nid_postfix(&second, u'x');
    Check(uhash_compareUnicodeString_nid_postfix(a, b) == 0);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&first);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&second);
    return 0;
}
