#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::printf("ICU TrieBuilder check failed at line %u\n", location.line());
    std::exit(1);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

void BasicTrieBuild() {
    std::int32_t status = 0;
    alignas(8) unsigned char raw[128];
    auto* builder = reinterpret_cast<UCharsTrieBuilder*>(raw);
    _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(builder, &status);
    Check(status == 0);

    alignas(8) unsigned char strRaw1[64];
    alignas(8) unsigned char strRaw2[64];
    alignas(8) unsigned char strRaw3[64];
    auto* s1 = reinterpret_cast<UnicodeString*>(strRaw1);
    auto* s2 = reinterpret_cast<UnicodeString*>(strRaw2);
    auto* s3 = reinterpret_cast<UnicodeString*>(strRaw3);
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(s1, u"apple");
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(s2, u"app");
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(s3, u"banana");

    _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s1, 10, &status);
    Check(status == 0);
    _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s2, 20, &status);
    Check(status == 0);
    _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s3, 30, &status);
    Check(status == 0);

    alignas(8) unsigned char outStrRaw[64]{};
    auto* outStr = reinterpret_cast<UnicodeString*>(outStrRaw);
    outStr->storage.stack.lengthAndFlags = 2;

    _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(builder, 0, outStr, &status);
    Check(status == 0);
    Check(outStr->Length() > 0);

    auto* trie = _ZN6icu_6817UCharsTrieBuilder5buildE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(builder, 0, &status);
    Check(status == 0);
    Check(trie != nullptr);

    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'a') == 1);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'p') == 1);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'p') == 3);

    _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(trie);
    Icu::Abi::Free_nid_no_patch(trie);

    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(outStr);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(s1);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(s2);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(s3);
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(builder);
}

void DuplicateKeyRejection() {
    std::int32_t status = 0;
    alignas(8) unsigned char raw[128];
    auto* builder = reinterpret_cast<UCharsTrieBuilder*>(raw);
    _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(builder, &status);

    alignas(8) unsigned char strRaw[64];
    auto* s = reinterpret_cast<UnicodeString*>(strRaw);
    _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(s, u"test");

    _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s, 1, &status);
    _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s, 2, &status);

    alignas(8) unsigned char outStrRaw[64]{};
    auto* outStr = reinterpret_cast<UnicodeString*>(outStrRaw);
    outStr->storage.stack.lengthAndFlags = 2;

    _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(builder, 0, outStr, &status);
    Check(status == 1);

    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(outStr);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(s);
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(builder);
}

void AddWords(UCharsTrieBuilder* builder, std::initializer_list<const char16_t*> words) {
    std::int32_t value = 0;
    for (const auto* word : words) {
        alignas(8) unsigned char strRaw[64];
        auto* s = reinterpret_cast<UnicodeString*>(strRaw);
        _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(s, word);
        std::int32_t status = 0;
        _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(builder, s, ++value, &status);
        Check(status == 0);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(s);
    }
}

void FailedGrowthStaysFailed() {
    std::int32_t status = 0;
    alignas(8) unsigned char raw[128];
    auto* builder = reinterpret_cast<UCharsTrieBuilder*>(raw);
    _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(builder, &status);
    AddWords(builder, {u"ab"});
    alignas(8) unsigned char outStrRaw[64]{};
    auto* outStr = reinterpret_cast<UnicodeString*>(outStrRaw);
    outStr->storage.stack.lengthAndFlags = 2;
    _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(builder, 0, outStr, &status);
    Check(status == 0);
    const auto length = builder->ucharsLength;
    Check(length > 0);
    Check(_ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(builder, std::numeric_limits<std::int32_t>::max()) == 0);
    Check(builder->uchars == nullptr);
    Check(_ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(builder, 1) == 0);
    Check(_ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(builder, u'x') == length);
    Check(builder->uchars == nullptr);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(outStr);
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(builder);
}

void OverflowingWriteFails() {
    std::int32_t status = 0;
    alignas(8) unsigned char raw[128];
    auto* builder = reinterpret_cast<UCharsTrieBuilder*>(raw);
    _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(builder, &status);
    AddWords(builder, {u"ab"});
    alignas(8) unsigned char outStrRaw[64]{};
    auto* outStr = reinterpret_cast<UnicodeString*>(outStrRaw);
    outStr->storage.stack.lengthAndFlags = 2;
    _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(builder, 0, outStr, &status);
    Check(status == 0);
    const auto length = builder->ucharsLength;
    const char16_t unit = u'x';
    Check(_ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(builder, &unit, std::numeric_limits<std::int32_t>::max()) == length);
    Check(builder->uchars == nullptr);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(outStr);
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(builder);
}

void ElementIndexesAreChecked() {
    std::int32_t status = 0;
    alignas(8) unsigned char raw[128];
    auto* builder = reinterpret_cast<UCharsTrieBuilder*>(raw);
    _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(builder, &status);
    AddWords(builder, {u"abc", u"abd", u"x"});
    Check(_ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(builder, 0, 1, 0) == 2);
    Check(_ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix(builder, 0, 2, 2) == 2);
    Check(_ZNK6icu_6817UCharsTrieBuilder26indexOfElementWithNextUnitEiiDs_nid_postfix(builder, 0, 0, u'a') == 2);
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(builder, 0, 3, 0); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(builder, -1, 1, 0); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(builder, 0, 2, 1); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix(builder, 0, 4, 0); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix(builder, 0, 3, 1); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder23skipElementsBySomeUnitsEiii_nid_postfix(builder, 2, 0, 1); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder26indexOfElementWithNextUnitEiiDs_nid_postfix(builder, 2, 0, u'x'); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder14getElementUnitEii_nid_postfix(builder, 2, 1); });
    Invalid([&] { _ZNK6icu_6817UCharsTrieBuilder21createLinearMatchNodeEiiiPNS_17StringTrieBuilder4NodeE_nid_postfix(builder, 0, 1, 3, nullptr); });
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(builder);
}

void NullChecks() {
    Invalid([] { _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(nullptr, nullptr); });
    Invalid([] { _ZN6icu_6817StringTrieBuilderC2Ev_nid_postfix(nullptr); });
    Invalid([] { _ZN6icu_6817StringTrieBuilder8hashNodeEPKv_nid_postfix(nullptr); });
    Invalid([] { _ZN6icu_6817StringTrieBuilder10equalNodesEPKvS2__nid_postfix(nullptr, nullptr); });
}

}

int main() {
    BasicTrieBuild();
    DuplicateKeyRejection();
    NullChecks();
    FailedGrowthStaysFailed();
    OverflowingWriteFails();
    ElementIndexesAreChecked();
    return 0;
}
