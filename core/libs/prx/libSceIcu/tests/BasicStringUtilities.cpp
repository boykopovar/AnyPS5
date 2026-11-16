// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <cstring>

namespace {
void Check(bool value) { if (!value) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    UnicodeString result{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&result, nullptr, 0);
    _ZN6icu_6811ICU_Utility12appendNumberERNS_13UnicodeStringEiii_nid_postfix(&result, -31, 16, 4);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&result) == 5 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result, 0) == u'-' && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result, 3) == u'1' && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result, 4) == u'F');
    Check(_ZN6icu_6811ICU_Utility17escapeUnprintableERNS_13UnicodeStringEi_nid_postfix(&result, 10) == 1);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&result) == 11 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result, 5) == u'\\' && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&result, 10) == u'A');
    UnicodeString input{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&input, u" \t\u200ex", 4);
    std::int32_t position = 0;
    Check(_ZN6icu_6811ICU_Utility14skipWhitespaceERKNS_13UnicodeStringERia_nid_postfix(&input, &position, 0) == 3 && position == 0);
    Check(_ZN6icu_6811ICU_Utility9parseCharERKNS_13UnicodeStringERiDs_nid_postfix(&input, &position, u'y') == 0 && position == 0);
    Check(_ZN6icu_6811ICU_Utility9parseCharERKNS_13UnicodeStringERiDs_nid_postfix(&input, &position, u'x') == 1 && position == 4);
    StringPiece hay{"abcabc", 6}, needle{"bc", 2};
    Check(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&hay, needle, 2) == 4);
    MaybeStackArray40 array{};
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&array);
    std::memcpy(array.pointer, "abc", 3);
    std::int32_t capacity = -77;
    auto* copy = _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(&array, 3, &capacity);
    Check(copy && capacity == 3 && std::memcmp(copy, "abc", 3) == 0 && array.pointer == array.stack);
    _ZN6icu_687UMemorydlEPv_nid_postfix(copy);
    UnicodeString rule{}, quoteBuf{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&rule, nullptr, 0);
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&quoteBuf, nullptr, 0);
    _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEiaaS2__nid_postfix(&rule, 'a', 0, 0, &quoteBuf);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&rule) == 1 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&rule, 0) == u'a');
    UnicodeString word{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&word, u"bc", 2);
    _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringERKS1_aaS2__nid_postfix(&rule, &word, 0, 0, &quoteBuf);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&rule) == 3 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&rule, 2) == u'c');
    _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEPKNS_14UnicodeMatcherEaS2__nid_postfix(&rule, nullptr, 0, &quoteBuf);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&rule) == 3);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&word);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&quoteBuf);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&rule);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&input);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&result);
    return 0;
}
