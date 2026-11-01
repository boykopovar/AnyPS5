#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::printf("ICU bytes trie check failed at line %u\n", location.line());
    std::exit(1);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

struct Sink { ByteSink base; std::string text; };

void APS5_VABI Append(ByteSink* sink, const char* text, std::int32_t length) {
    Check(length == 1);
    reinterpret_cast<Sink*>(sink)->text.append(text, length);
}

const ByteSinkFunctions SinkFunctions{nullptr, nullptr, Append, nullptr, nullptr};

void Matching() {
    const std::uint8_t bytes[]{0x11, 'a', 'b', 0x2f};
    BytesTrie trie{nullptr, bytes, bytes, -1};
    Check(_ZNK6icu_689BytesTrie7currentEv_nid_postfix(&trie) == 1);
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'a') == 1 && trie.remaining == 0);
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'b') == 2 && trie.remaining == -1);
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'c') == 0 && !trie.position);
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'a') == 0);
    for (auto length : {-9, 2}) {
        trie = {nullptr, bytes, bytes, -1};
        Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, "ab", length) == 2);
        Check(trie.position == bytes + 3 && trie.remaining == -1);
    }
    trie = {nullptr, bytes, bytes, -1};
    Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, nullptr, 0) == 1);
    Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, "ax", 2) == 0);
    Check(trie.remaining == -1);
    const std::uint8_t intermediate[]{0x10, 'a', 0x2a, 0x10, 'b', 0x2f};
    trie = {nullptr, intermediate, intermediate, -1};
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'a') == 3);
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, 'b') == 2);
    trie = {nullptr, intermediate, intermediate, -1};
    Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, "ab", 2) == 2);
    const std::uint8_t high[]{0x10, 0xff, 0x23};
    trie = {nullptr, high, high, -1};
    Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, -1) == 2);
    trie = {nullptr, high, high, -1};
    const char signedByte[]{static_cast<char>(0xff), 0};
    Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, signedByte, 1) == 0);
    trie = {nullptr, bytes, bytes, -1};
    Check(_ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(&trie, bytes, 'a') == 1);
    Invalid([&] { _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, nullptr, -1); });
    Invalid([] { _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(nullptr, nullptr, 0); });
    _ZN6icu_689BytesTrieD1Ev_nid_postfix(&trie);
    _ZN6icu_689BytesTrieD2Ev_nid_postfix(&trie);
}

void Branches() {
    const std::uint8_t bytes[]{1, 'a', 0x24, 'b', 0x23, 0x10, 'x', 0x25};
    BytesTrie trie{nullptr, bytes, bytes, -1};
    Check(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&trie, "ax", -1) == 2 && trie.position == bytes + 7);
    trie = {nullptr, bytes, bytes, -1};
    Check(_ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(&trie, bytes + 1, 1, 'b') == 2);
    const std::uint8_t tree[]{5, 'd', 6, 'd', 0x23, 'e', 0x23, 'f', 0x23, 'a', 0x23, 'b', 0x23, 'c', 0x23};
    for (int value = 'a'; value <= 'g'; ++value) {
        trie = {nullptr, tree, tree, -1};
        Check(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&trie, value) == (value == 'g' ? 0 : 2));
    }
    Sink sink{{&SinkFunctions}, {}};
    trie = {nullptr, tree, tree, -1};
    Check(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(&trie, &sink.base) == 6 && sink.text == "abcdef");
    sink.text.clear();
    _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(tree + 1, 6, &sink.base);
    Check(sink.text == "abcdef");
    sink.text.clear();
    _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(&sink.base, 0x141);
    Check(sink.text == "A");
    std::int32_t value = 9;
    Check(_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(tree, 0, &value) && value == 1);
    Check(_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(tree, -1, &value));
    value = 99;
    Check(!_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(tree, -1, &value) && value == 99);
    value = 0;
    Check(_ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(tree + 1, 6, 0, &value) == tree + 8 && value == 1);
    const std::uint8_t different[]{1, 'a', 0x23, 'b', 0x25};
    Check(!_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(different, 0, &value));
    trie = {nullptr, bytes, bytes + 5, -1};
    sink.text.clear();
    Check(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(&trie, &sink.base) == 1 && sink.text == "x");
    trie.position = bytes + 6;
    trie.remaining = 0;
    Check(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(&trie, &sink.base) == 1);
    trie.position = nullptr;
    Check(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(&trie, nullptr) == 0);
    Invalid([] { _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(nullptr, 0); });
}

void Values() {
    const std::uint8_t bytes[]{0x12, 0x34, 0x56, 0x78};
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x10) == 0);
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x50) == 64);
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(bytes, 0x51) == 0x12);
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(bytes, 0x6c) == 0x1234);
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(bytes, 0x7e) == 0x123456);
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(bytes, 0x7f) == 0x12345678);
    const std::uint8_t negative[]{0xff, 0xff, 0xff, 0xff};
    Check(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(negative, 0x7f) == -1);
    const std::uint8_t delta[]{0xc0, 1, 0, 0};
    Check(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta) == delta + 3);
    Invalid([] { _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x51); });
    Invalid([] { _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(nullptr); });
}

}

int main() {
    Matching();
    Branches();
    Values();
    std::puts("ICU bytes trie tests passed");
}
