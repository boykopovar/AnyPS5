#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU Trie2 check failed at line %u\n", location.line());
    std::exit(1);
}

template<class TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

void Dummy() {
    for (int width = 0; width <= 1; ++width) {
        std::int32_t status = -128;
        auto* trie = utrie2_openDummy_nid_postfix(width, 0x12345678, 0x87654321, &status);
        const auto initialValue = width == 0 ? 0x5678u : 0x12345678u;
        const auto errorValue = width == 0 ? 0x4321u : 0x87654321u;
        Require(trie && status == -128 && utrie2_isFrozen_nid_postfix(trie));
        Require(trie->indexLength == 2112 && trie->dataLength == 196 && trie->highStart == 0 && trie->isMemoryOwned == 1);
        for (int character = -1; character <= 0x110000; ++character) {
            Require(utrie2_get32_nid_postfix(trie, character) == (character < 0 || character > 0x10ffff ? errorValue : initialValue));
            Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, character) ==
                (character >= 0xd800 && character <= 0xdbff ? initialValue : 0x87654321));
        }
        for (int character : {std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
            Require(utrie2_get32_nid_postfix(trie, character) == errorValue);
        status = 0;
        const auto size = utrie2_serialize_nid_postfix(trie, nullptr, 0, &status);
        Require(status == 15 && size == 16 + 2112 * 2 + 196 * (width == 0 ? 2 : 4));
        std::vector<std::uint32_t> serialized(size / 4 + 1, 0x12345678);
        status = 0;
        Require(utrie2_serialize_nid_postfix(trie, serialized.data(), size - 1, &status) == size && status == 15);
        Require(serialized.front() == 0x12345678);
        status = -128;
        Require(utrie2_serialize_nid_postfix(trie, serialized.data(), size + 4, &status) == size && status == -128);
        const auto* header = reinterpret_cast<const std::uint16_t*>(serialized.data());
        const auto move = width == 0 ? 2112 : 0;
        const std::array<std::uint16_t, 8> expectedHeader{0x6932, 0x5472, static_cast<std::uint16_t>(width), 2112, 49, 0,
            static_cast<std::uint16_t>(move), 0};
        Require(std::equal(expectedHeader.begin(), expectedHeader.end(), header));
        const auto* index = header + 8;
        for (int offset = 0; offset < 2112; ++offset)
            Require(index[offset] == (offset < 2080 ? move >> 2 : offset < 2082 ? move + 128 : move));
        for (int offset = 0; offset < 196; ++offset) {
            const auto value = width == 0 ? index[2112 + offset] : reinterpret_cast<const std::uint32_t*>(index + 2112)[offset];
            Require(value == (offset >= 128 && offset < 192 ? errorValue : initialValue));
        }
        Require(serialized.back() == 0x12345678);
        std::int32_t actual = -1;
        auto* reopened = utrie2_openFromSerialized_nid_postfix(width, serialized.data(), size + 4, &actual, &status);
        Require(reopened && actual == size && status == -128 && !reopened->isMemoryOwned);
        Require(reopened->initialValue == initialValue && reopened->errorValue == errorValue);
        Require(utrie2_get32_nid_postfix(reopened, 0x10ffff) == initialValue);
        std::vector<std::uint32_t> roundTrip(size / 4);
        Require(utrie2_serialize_nid_postfix(reopened, roundTrip.data(), size, &status) == size);
        Require(std::memcmp(serialized.data(), roundTrip.data(), size) == 0);
        utrie2_close_nid_postfix(reopened);
        utrie2_close_nid_postfix(trie);
    }
}

void Structured() {
    for (int width = 0; width <= 1; ++width) {
        const int move = width == 0 ? 2208 : 0;
        std::vector<std::uint32_t> binary((16 + 2208 * 2 + 256 * (width == 0 ? 2 : 4)) / 4);
        auto* header = reinterpret_cast<std::uint16_t*>(binary.data());
        const std::array<std::uint16_t, 8> fields{0x6932, 0x5472, static_cast<std::uint16_t>(0xfff0 | width),
            2208, 64, 0xffff, static_cast<std::uint16_t>(move), 33};
        std::copy(fields.begin(), fields.end(), header);
        auto* index = header + 8;
        std::fill_n(index, 2048, move >> 2);
        std::fill_n(index + 2048, 32, (move + 32) >> 2);
        index[2112] = 2140;
        std::fill_n(index + 2140, 64, (move + 64) >> 2);
        for (int offset = 0; offset < 256; ++offset) {
            const auto value = offset == 128 ? 99 : offset == 252 ? 13 : offset >= 64 && offset < 96 ? 9 :
                offset >= 32 && offset < 64 ? 7 : 5;
            if (width == 0) index[2208 + offset] = value;
            else reinterpret_cast<std::uint32_t*>(index + 2208)[offset] = value;
        }
        std::int32_t status = 0;
        auto* trie = utrie2_openFromSerialized_nid_postfix(width, binary.data(), binary.size() * 4, nullptr, &status);
        Require(trie && status == 0);
        for (int character = -1; character <= 0x110000; ++character) {
            const auto expected = character < 0 || character > 0x10ffff ? 99 : character >= 0x10800 ? 13 :
                character >= 0x10000 ? 9 : character >= 0xd800 && character <= 0xdbff ? 7 : 5;
            Require(utrie2_get32_nid_postfix(trie, character) == static_cast<unsigned>(expected));
        }
        for (int character = 0xd800; character <= 0xdbff; ++character)
            Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, character) == 5);
        const std::array<std::uint8_t, 4> utf8{0xf0, 0x90, 0x80, 0x81};
        Require(utrie2_internalU8NextIndex_nid_postfix(trie, 0xf0, utf8.data() + 1, utf8.data() + 4) == ((move + 65) << 3 | 3));
        Require(utrie2_internalU8PrevIndex_nid_postfix(trie, 0x81, utf8.data(), utf8.data() + 3) == ((move + 65) << 3 | 3));
        Require(utrie2_internalU8NextIndex_nid_postfix(trie, 0xf0, utf8.data() + 1, utf8.data() + 3) == ((move + 128) << 3 | 2));
        Require(utrie2_internalU8PrevIndex_nid_postfix(trie, 0x80, utf8.data(), utf8.data() + 2) == ((move + 128) << 3 | 2));
        utrie2_close_nid_postfix(trie);
    }
    auto builder = std::make_unique<Icu::NewTrie2>();
    std::array<std::uint32_t, 100> values;
    std::fill_n(values.data(), 32, 5);
    std::fill_n(values.data() + 32, 32, 7);
    std::fill(values.begin() + 64, values.end(), 13);
    builder->data = values.data();
    builder->dataLength = values.size();
    builder->highStart = 0x10000;
    builder->index2[2048] = 32;
    Icu::Trie2 trie{};
    trie.builder = builder.get();
    trie.errorValue = 99;
    Require(!utrie2_isFrozen_nid_postfix(&trie));
    Require(utrie2_get32_nid_postfix(&trie, 0) == 5);
    Require(utrie2_get32_nid_postfix(&trie, 0xd800) == 7);
    Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(&trie, 0xd800) == 5);
    Require(utrie2_get32_nid_postfix(&trie, 0x10000) == 13);
    Require(utrie2_get32_nid_postfix(&trie, -1) == 99);
    builder->highStart = 0;
    Require(utrie2_get32_nid_postfix(&trie, 0xd800) == 13);
    Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(&trie, 0xd800) == 5);
    std::int32_t status = 0;
    Require(utrie2_serialize_nid_postfix(&trie, nullptr, 0, &status) == 0 && status == 1);
}

void Errors() {
    std::int32_t status = 7;
    Require(!utrie2_openDummy_nid_postfix(-1, 0, 0, &status) && status == 7);
    Require(!utrie2_openFromSerialized_nid_postfix(-1, nullptr, -1, nullptr, &status) && status == 7);
    Require(utrie2_serialize_nid_postfix(nullptr, nullptr, -1, &status) == 0 && status == 7);
    for (int width : {-1, 2, 3}) {
        status = 0;
        Require(!utrie2_openDummy_nid_postfix(width, 0, 0, &status) && status == 1);
    }
    status = 0;
    auto* trie = utrie2_openDummy_nid_postfix(0, 1, 2, &status);
    std::vector<std::uint32_t> serialized(trie->length / 4);
    Require(utrie2_serialize_nid_postfix(trie, serialized.data(), trie->length, &status) == trie->length);
    for (int length : {-1, 0, 1, 15, 16, trie->length - 1}) {
        std::int32_t actual = 123;
        status = 0;
        Require(!utrie2_openFromSerialized_nid_postfix(0, serialized.data(), length, &actual, &status));
        Require(status == (length <= 0 ? 1 : 3) && actual == 123);
    }
    status = 0;
    Require(!utrie2_openFromSerialized_nid_postfix(1, serialized.data(), trie->length, nullptr, &status) && status == 3);
    serialized[0] = 0x32697254;
    status = 0;
    Require(!utrie2_openFromSerialized_nid_postfix(0, serialized.data(), trie->length, nullptr, &status) && status == 3);
    for (int capacity : {-1, 1}) {
        status = 0;
        Require(utrie2_serialize_nid_postfix(trie, nullptr, capacity, &status) == 0 && status == 1);
    }
    status = 0;
    Require(utrie2_serialize_nid_postfix(nullptr, nullptr, 0, &status) == 0 && status == 1);
    status = 0;
    Require(utrie2_serialize_nid_postfix(trie, reinterpret_cast<char*>(serialized.data()) + 1, trie->length, &status) == 0 && status == 1);
    status = 0;
    Invalid([&] { utrie2_openFromSerialized_nid_postfix(0, nullptr, 16, nullptr, &status); });
    Invalid([&] { utrie2_openDummy_nid_postfix(0, 0, 0, nullptr); });
    Invalid([&] { utrie2_get32_nid_postfix(nullptr, 0); });
    Invalid([&] { utrie2_isFrozen_nid_postfix(nullptr); });
    Invalid([&] { utrie2_serialize_nid_postfix(trie, nullptr, 0, nullptr); });
    utrie2_close_nid_postfix(trie);
    utrie2_close_nid_postfix(nullptr);
}

}

int main() {
    Dummy();
    Structured();
    Errors();
    std::puts("ICU Trie2 tests passed");
}
