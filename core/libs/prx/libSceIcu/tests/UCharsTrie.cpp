#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::printf("ICU UCharsTrie check failed at line %u\n", location.line());
    std::exit(1);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

struct Sink { Appendable base; std::u16string text; int reserved = 0; };

std::int8_t APS5_VABI Append(Appendable* sink, char16_t unit) {
    reinterpret_cast<Sink*>(sink)->text.push_back(unit);
    return 0;
}

std::int8_t APS5_VABI Reserve(Appendable* sink, std::int32_t length) {
    reinterpret_cast<Sink*>(sink)->reserved = length;
    return 0;
}

const AppendableFunctions SinkFunctions{nullptr, nullptr, nullptr, Append, nullptr, nullptr, Reserve, nullptr};

void Matching() {
    const char16_t units[]{0x31, u'a', u'b', 0x8007};
    UCharsTrie trie{nullptr, units, units, -1};
    Check(_ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(&trie) == 1);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'a') == 1 && trie.remaining == 0);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'b') == 2 && trie.remaining == -1);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'c') == 0 && !trie.position);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'a') == 0);
    for (int length : {-1, 2}) {
        trie = {nullptr, units, units, -1};
        ConstChar16Ptr text{u"ab"};
        Check(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &text, length) == 2);
        Check(trie.position == units + 3 && trie.remaining == -1);
    }
    trie = {nullptr, units, units, -1};
    ConstChar16Ptr empty{nullptr};
    Check(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &empty, 0) == 1);
    Invalid([&] { _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &empty, -1); });
    ConstChar16Ptr mismatch{u"ax"};
    Check(!_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &mismatch, 2) && trie.remaining == -1);
    const char16_t intermediate[]{0x30, u'a', 0x1b0, u'b', 0x8007};
    trie = {nullptr, intermediate, intermediate, -1};
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'a') == 3);
    Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, u'b') == 2);
    trie = {nullptr, intermediate, intermediate, -1};
    ConstChar16Ptr ab{u"ab"};
    Check(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &ab, 2) == 2);
    trie = {nullptr, units, units, -1};
    Check(_ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(&trie, units, u'a') == 1);
    const char16_t supplementary[]{0x31, 0xd83d, 0xde00, 0x8005};
    trie = {nullptr, supplementary, nullptr, 0};
    Check(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(&trie, 0x1f600) == 2);
    trie = {nullptr, supplementary, supplementary, -1};
    Check(_ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(&trie, 0x1f600) == 2);
    const char16_t wrapped[]{0x31, 0xd7bf, 0xdfff, 0x8003};
    trie = {nullptr, wrapped, wrapped, -1};
    Check(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(&trie, std::numeric_limits<std::int32_t>::max()) == 2);
    trie = {nullptr, wrapped, wrapped, -1};
    Check(!_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(&trie, -1));
    _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(&trie);
    _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(&trie);
}

void Branches() {
    const char16_t units[]{1, u'a', 2, u'b', 0x8001, 0x30, u'x', 0x8002};
    UCharsTrie trie{nullptr, units, units, -1};
    ConstChar16Ptr ax{u"ax"};
    Check(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&trie, &ax, -1) == 2 && trie.position == units + 7);
    trie = {nullptr, units, units, -1};
    Check(_ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(&trie, units + 1, 1, u'b') == 2);
    const char16_t tree[]{5, u'd', 6, u'd', 0x8001, u'e', 0x8001, u'f', 0x8001, u'a', 0x8001, u'b', 0x8001, u'c', 0x8001};
    for (int unit = u'a'; unit <= u'g'; ++unit) {
        trie = {nullptr, tree, tree, -1};
        Check(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&trie, unit) == (unit == u'g' ? 0 : 2));
    }
    Sink sink{{&SinkFunctions}, {}, 0};
    trie = {nullptr, tree, tree, -1};
    Check(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(&trie, &sink.base) == 6 && sink.text == u"abcdef" && sink.reserved == 6);
    sink.text.clear();
    _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(tree + 1, 6, &sink.base);
    Check(sink.text == u"abcdef");
    std::int32_t value = 99;
    Check(_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(tree, 0, &value) && value == 1);
    Check(_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(tree, -3, &value));
    value = 99;
    Check(!_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(tree, -3, &value) && value == 99);
    value = 0;
    Check(_ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(tree + 1, 6, 0, &value) == tree + 8 && value == 1);
    const char16_t different[]{1, u'a', 0x8001, u'b', 0x8002};
    Check(!_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(different, 0, &value));
    const char16_t negative[]{0xffff, 0xffff, 0xffff};
    Check(_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(negative, 0, &value) && value == -1);
    const char16_t intermediate[]{0x30, u'a', 0x1b0, u'b', 0x8006};
    Check(!_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(intermediate, 0, &value));
    trie = {nullptr, units, units + 5, -1};
    sink.text.clear();
    Check(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(&trie, &sink.base) == 1 && sink.text == u"x");
    trie.position = units + 6;
    trie.remaining = 0;
    Check(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(&trie, &sink.base) == 1);
    trie.position = nullptr;
    Check(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(&trie, nullptr) == 0);
    Invalid([] { _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(nullptr, 0, nullptr); });
}

}

int main() {
    Matching();
    Branches();
    std::puts("ICU UCharsTrie tests passed");
}
