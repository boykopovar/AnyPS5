// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    void* functorId = _ZN6icu_6814UnicodeFunctor16getStaticClassIDEv_nid_postfix();
    void* filterId = _ZN6icu_6813UnicodeFilter16getStaticClassIDEv_nid_postfix();
    Check(functorId != nullptr);
    Check(filterId != nullptr);
    Check(functorId != filterId);

    Icu::Abi::UnicodeFunctor functor{};
    Check(_ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(&functor) == nullptr);
    Check(_ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(&functor) == nullptr);
    _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(&functor);

    Icu::Abi::UnicodeMatcher matcher{};
    _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(&matcher);

    Icu::Abi::SymbolTable table{};
    _ZN6icu_6811SymbolTableD1Ev_nid_postfix(&table);

    Icu::Abi::UnicodeSet set{};
    _ZN6icu_6810UnicodeSetC1Ev_nid_postfix(&set);
    _ZN6icu_6810UnicodeSet3addEii_nid_postfix(&set, 'a', 'z');

    auto* filter = reinterpret_cast<Icu::Abi::UnicodeFilter*>(&set);
    Check(_ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(filter) == reinterpret_cast<const char*>(&set) + 8);
    _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(filter, nullptr);

    Icu::Abi::UnicodeString str{};
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&str, u"abc");

    std::int32_t offset = 0;
    std::int32_t res = _ZN6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(filter, &str, &offset, 3, 0);
    Check(res == 2 && offset == 1);

    offset = 1;
    res = _ZThn8_N6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(reinterpret_cast<const char*>(&set) + 8, &str, &offset, 3, 0);
    Check(res == 2 && offset == 2);

    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&str);
    _ZN6icu_6810UnicodeSetD1Ev_nid_postfix(&set);

    try {
        _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) {}

    std::puts("ICU UnicodeFilter tests passed");
    return 0;
}
