#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU mutable code point trie check failed at line %u\n", location.line());
    std::abort();
}

template<class TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

std::uint32_t APS5_VABI Parity_nid_no_patch(const void* context, std::uint32_t value) {
    static_cast<std::vector<std::uint32_t>*>(const_cast<void*>(context))->push_back(value);
    return value & 1;
}

void Mutations() {
    std::int32_t status = -128;
    auto* trie = umutablecptrie_open_nid_postfix(0x12345678, 0x87654321, &status);
    Require(trie && status == -128 && trie->indexCapacity == 4096 && trie->dataCapacity == 16384);
    std::vector<std::uint32_t> model(0x110000, 0x12345678);
    Require(umutablecptrie_get_nid_postfix(trie, -1) == 0x87654321);
    Require(umutablecptrie_get_nid_postfix(trie, 0x110000) == 0x87654321);
    std::uint32_t value = 0;
    std::vector<std::uint32_t> calls;
    Require(umutablecptrie_getRange_nid_postfix(trie, 0, 0, 0, Parity_nid_no_patch, &calls, nullptr) == 0x10ffff && calls.empty());
    Require(umutablecptrie_getRange_nid_postfix(trie, 0, 0, 0, Parity_nid_no_patch, &calls, &value) == 0x10ffff);
    Require(value == 0 && calls == std::vector<std::uint32_t>({0x12345678}));
    const auto set = [&](int character, std::uint32_t entry) {
        umutablecptrie_set_nid_postfix(trie, character, entry, &status);
        Require(status == -128);
        model[character] = entry;
    };
    const auto range = [&](int start, int end, std::uint32_t entry) {
        umutablecptrie_setRange_nid_postfix(trie, start, end, entry, &status);
        Require(status == -128);
        std::fill(model.begin() + start, model.begin() + end + 1, entry);
    };
    set(1, 3);
    Require(trie->highStart == 512 && trie->dataLength == 64);
    set(17, 5);
    Require(trie->dataLength == 64);
    range(3, 7, 7);
    range(15, 16, 9);
    range(31, 64, 11);
    range(0xd7f0, 0xe010, 13);
    range(0x10000, 0x10100, 15);
    set(0x10ffff, 17);
    Require(trie->indexCapacity == 0x11000 && trie->highStart == 0x110000);
    for (int character = 0; character < 0x10000; character += 64) set(character, character);
    Require(trie->dataCapacity == 0x20000);
    for (int character = 0x10000; character < 0x110000; character += 16) set(character, character ^ 0x80000000u);
    Require(trie->dataCapacity == 0x110000);
    for (int character = 0; character < 0x110000; ++character)
        Require(umutablecptrie_get_nid_postfix(trie, character) == model[character]);
    auto* clone = umutablecptrie_clone_nid_postfix(trie, &status);
    const auto cloneModel = model;
    Require(clone && status == -128 && clone->dataLength == trie->dataLength && clone->highStart == trie->highStart);
    Require(clone->index != trie->index && clone->data != trie->data);
    range(0, 0x10ffff, 23);
    Require(umutablecptrie_getRange_nid_postfix(trie, 0, 0, 0, nullptr, nullptr, &value) == 0x10ffff && value == 23);
    for (int character = 0; character < 0x110000; ++character)
        Require(umutablecptrie_get_nid_postfix(clone, character) == cloneModel[character]);
    for (int start : {0, 1, 2, 15, 16, 31, 64, 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000, 0xffff,
                      0x10000, 0x10fffe, 0x10ffff}) {
        for (int option : {0, 1, 2, 99}) {
            const auto expectedAt = [&](int character) {
                return option != 0 && character >= 0xd800 && character <= (option == 2 ? 0xdfff : 0xdbff) ?
                    42u : cloneModel[character];
            };
            int end = start;
            while (end < 0x10ffff && expectedAt(end + 1) == expectedAt(start)) ++end;
            Require(umutablecptrie_getRange_nid_postfix(clone, start, option, 42, nullptr, nullptr, &value) == end);
            Require(value == expectedAt(start));
        }
    }
    calls.clear();
    Require(umutablecptrie_getRange_nid_postfix(clone, 1, 0, 0, Parity_nid_no_patch, &calls, &value) == 1);
    Require(calls == std::vector<std::uint32_t>({0x12345678, 3}) && value == 1);
    umutablecptrie_close_nid_postfix(clone);
    umutablecptrie_close_nid_postfix(trie);
}

void Conversions() {
    for (int width = 0; width <= 2; ++width) {
        std::array<std::uint16_t, 64> index{};
        index[1] = 64;
        std::array<std::uint32_t, 132> dwords{};
        std::array<std::uint16_t, 132> words{};
        std::array<std::uint8_t, 132> bytes{};
        for (int offset = 0; offset < 132; ++offset) {
            const auto value = offset == 130 ? 9 : offset == 131 ? 99 : offset % 7;
            dwords[offset] = words[offset] = bytes[offset] = value;
        }
        Icu::CodePointTrie source{};
        source.index = index.data();
        source.indexLength = index.size();
        source.dataLength = 132;
        source.highStart = 0x1000;
        source.type = 1;
        source.valueWidth = width;
        source.dataNullOffset = 0xfffff;
        source.index3NullOffset = 0x7fff;
        source.nullValue = 9;
        if (width == 0) source.data.words = words.data();
        else if (width == 1) source.data.dwords = dwords.data();
        else source.data.bytes = bytes.data();
        for (bool map : {false, true}) {
            std::int32_t status = 0;
            auto* trie = map ? umutablecptrie_fromUCPMap_nid_postfix(&source, &status) :
                umutablecptrie_fromUCPTrie_nid_postfix(&source, &status);
            Require(trie && status == 0 && trie->initialValue == 9 && trie->errorValue == 99);
            for (int character = -1; character <= 0x110000; ++character) {
                const auto expected = character < 0 || character == 0x110000 ? 99u : character >= 0x1000 ? 9u :
                    static_cast<unsigned>(character <= 127 ? character % 7 : (character & 63) % 7);
                Require(umutablecptrie_get_nid_postfix(trie, character) == expected);
            }
            umutablecptrie_close_nid_postfix(trie);
        }
    }
}

void Errors() {
    std::int32_t status = 7;
    Require(!umutablecptrie_open_nid_postfix(0, 0, &status) && status == 7);
    Require(!umutablecptrie_clone_nid_postfix(nullptr, &status) && status == 7);
    Require(!umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &status) && status == 7);
    Require(!umutablecptrie_fromUCPMap_nid_postfix(nullptr, &status) && status == 7);
    umutablecptrie_set_nid_postfix(nullptr, 0, 0, &status);
    umutablecptrie_setRange_nid_postfix(nullptr, 0, 1, 0, &status);
    Require(status == 7);
    status = -128;
    Require(!umutablecptrie_clone_nid_postfix(nullptr, &status) && status == -128);
    Require(!umutablecptrie_fromUCPMap_nid_postfix(nullptr, &status) && status == 1);
    status = 0;
    Require(!umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &status) && status == 1);
    status = 0;
    auto* trie = umutablecptrie_open_nid_postfix(1, 2, &status);
    auto* clone = umutablecptrie_clone_nid_postfix(trie, &status);
    Require(clone && clone->highStart == 0 && clone->dataLength == 0);
    umutablecptrie_close_nid_postfix(clone);
    for (int character : {-1, 0x110000, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()}) {
        status = 0;
        umutablecptrie_set_nid_postfix(trie, character, 3, &status);
        Require(status == 1 && trie->highStart == 0);
        status = 0;
        umutablecptrie_setRange_nid_postfix(trie, 0, character, 3, &status);
        Require(status == 1 && trie->highStart == 0);
        std::uint32_t value = 456;
        Require(umutablecptrie_getRange_nid_postfix(trie, character, 1, 0, nullptr, nullptr, &value) == -1 && value == 456);
    }
    status = 0;
    umutablecptrie_setRange_nid_postfix(trie, 2, 1, 3, &status);
    Require(status == 1 && trie->highStart == 0);
    status = 0;
    Invalid([&] { umutablecptrie_set_nid_postfix(nullptr, 0, 0, &status); });
    Invalid([&] { umutablecptrie_get_nid_postfix(nullptr, 0); });
    Invalid([&] { umutablecptrie_open_nid_postfix(0, 0, nullptr); });
    Invalid([&] { umutablecptrie_clone_nid_postfix(trie, nullptr); });
    Invalid([&] { umutablecptrie_setRange_nid_postfix(trie, 0, 1, 3, nullptr); });
    umutablecptrie_close_nid_postfix(trie);
    umutablecptrie_close_nid_postfix(nullptr);
}

}

int main() {
    Mutations();
    Conversions();
    Errors();
    std::puts("ICU mutable code point trie tests passed");
}
