#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU code point trie check failed at line %u\n", location.line());
    std::abort();
}

template<class TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

constexpr std::int32_t HighStart = 0x20200;
constexpr std::int32_t IndexLength = 12000;
constexpr std::int32_t DataLength = 70020;

std::vector<std::uint32_t> Binary(std::int32_t type, std::int32_t width) {
    const auto valueBytes = width == 0 ? 2 : width == 1 ? 4 : 1;
    const auto size = 16 + IndexLength * 2 + DataLength * valueBytes;
    std::vector<std::uint32_t> result(size / 4);
    auto* header = reinterpret_cast<std::uint16_t*>(result.data());
    result[0] = 0x54726933;
    header[2] = ((DataLength & 0xf0000) >> 4) | (type << 6) | width;
    header[3] = IndexLength;
    header[4] = static_cast<std::uint16_t>(DataLength);
    header[5] = 0x7fff;
    header[6] = 0;
    header[7] = HighStart >> 9;
    auto* index = header + 8;
    index[1] = 64;
    index[2] = 128;
    for (int first = 0; first < 9; ++first) {
        index[(type == 0 ? 1020 : 64) + first] = 1100 + first * 32;
        for (int second = 0; second < 32; ++second) {
            const auto third = 1500 + (first * 32 + second) * 36;
            if (third + 36 > IndexLength) continue;
            index[1100 + first * 32 + second] = 0x8000 | third;
            for (int group = 0; group < 4; ++group) {
                index[third + group * 9] = 0x5555;
                for (int offset = 1; offset <= 8; ++offset)
                    index[third + group * 9 + offset] = static_cast<std::uint16_t>(70000);
            }
        }
    }
    auto* bytes = reinterpret_cast<std::uint8_t*>(index + IndexLength);
    for (int position = 0; position < DataLength; ++position) {
        const std::uint32_t value = position == 128 ? 7 : position == 129 ? 9 : position == 70001 ? 12 :
            position >= DataLength - 2 ? (position == DataLength - 2 ? 13 : 99) : position >= 70000 ? 11 : 5;
        if (width == 0) reinterpret_cast<std::uint16_t*>(bytes)[position] = value;
        else if (width == 1) reinterpret_cast<std::uint32_t*>(bytes)[position] = value;
        else bytes[position] = value;
    }
    return result;
}

std::uint32_t Expected(std::int32_t type, std::int32_t character) {
    if (static_cast<std::uint32_t>(character) > 0x10ffff) return 99;
    if (character <= 127) return 5;
    if (character <= (type == 0 ? 0xffff : 0xfff)) {
        if (character == 0x80) return 7;
        if (character == 0x81) return 9;
        return 5;
    }
    if (character >= HighStart) return 13;
    return (character & 15) == 1 ? 12 : 11;
}

std::uint32_t APS5_VABI Parity_nid_no_patch(const void* context, std::uint32_t value) {
    static_cast<std::vector<std::uint32_t>*>(const_cast<void*>(context))->push_back(value);
    return value & 1;
}

void ValidBinaries() {
    for (int type = 0; type <= 1; ++type) {
        for (int width = 0; width <= 2; ++width) {
            auto binary = Binary(type, width);
            std::int32_t status = -128;
            std::int32_t actual = -1;
            auto* trie = ucptrie_openFromBinary_nid_postfix(-1, -1, binary.data(), binary.size() * 4, &actual, &status);
            Require(trie && status == -128 && actual == static_cast<std::int32_t>(binary.size() * 4));
            Require(ucptrie_getType_nid_postfix(trie) == type && ucptrie_getValueWidth_nid_postfix(trie) == width);
            Require(trie->reserved16 == 0 && trie->reserved32 == 0 && trie->nullValue == 5);
            Require(trie->shifted12HighStart == 0x21 && trie->highStart == HighStart);
            for (int character = -1; character <= 0x110000; ++character) {
                Require(ucptrie_get_nid_postfix(trie, character) == Expected(type, character));
                Require(ucpmap_get_nid_postfix(trie, character) == Expected(type, character));
            }
            Require(ucptrie_get_nid_postfix(trie, std::numeric_limits<std::int32_t>::min()) == 99);
            Require(ucptrie_get_nid_postfix(trie, std::numeric_limits<std::int32_t>::max()) == 99);
            const int firstSmall = type == 0 ? 0x10000 : 0x1000;
            for (int character = firstSmall; character < HighStart; ++character)
                Require(ucptrie_internalSmallIndex_nid_postfix(trie, character) == 70000 + (character & 15));
            Require(ucptrie_internalSmallU8Index_nid_postfix(trie, 0x10, 0, 1) == 70001);
            Require(ucptrie_internalSmallU8Index_nid_postfix(trie, 0x20, 8, 0) == DataLength - 2);
            for (const int start : {0, 127, 128, 129, 130, 0xfff, 0x1000, 0xd7ff, 0xd800, 0xdbff,
                                   0xdc00, 0xdfff, 0xe000, 0xffff, 0x10000, 0x10001, HighStart - 1, HighStart, 0x10ffff}) {
                for (const int option : {0, 1, 2, 42}) {
                    const auto expectedAt = [&](int character) {
                        return option != 0 && character >= 0xd800 && character <= (option == 2 ? 0xdfff : 0xdbff) ?
                            23u : Expected(type, character);
                    };
                    const auto expectedValue = expectedAt(start);
                    int end = start;
                    while (end < 0x10ffff && expectedAt(end + 1) == expectedValue) ++end;
                    std::uint32_t value = 0;
                    Require(ucptrie_getRange_nid_postfix(trie, start, option, 23, nullptr, nullptr, &value) == end);
                    Require(value == expectedValue);
                    Require(ucpmap_getRange_nid_postfix(trie, start, option, 23, nullptr, nullptr, nullptr) == end);
                }
            }
            std::uint32_t value = 1234;
            Require(ucptrie_getRange_nid_postfix(nullptr, -1, 2, 0, nullptr, nullptr, &value) == -1 && value == 1234);
            Require(ucpmap_getRange_nid_postfix(nullptr, 0x110000, 0, 0, nullptr, nullptr, &value) == -1 && value == 1234);
            std::vector<std::uint32_t> calls;
            Require(ucptrie_getRange_nid_postfix(trie, 127, 0, 0, Parity_nid_no_patch, &calls, &value) == firstSmall);
            Require(value == 1 && calls == std::vector<std::uint32_t>({5, 7, 9, 11, 12}));
            calls.clear();
            Require(ucptrie_getRange_nid_postfix(trie, HighStart, 0, 0, Parity_nid_no_patch, &calls, nullptr) == 0x10ffff);
            Require(calls.empty());
            Require(ucptrie_getRange_nid_postfix(trie, HighStart, 0, 0, Parity_nid_no_patch, &calls, &value) == 0x10ffff);
            Require(calls == std::vector<std::uint32_t>({13}) && value == 1);
            status = 0;
            Require(ucptrie_toBinary_nid_postfix(trie, nullptr, 0, &status) == actual && status == 15);
            std::vector<std::uint32_t> output(binary.size() + 1, 0x12345678);
            status = 0;
            Require(ucptrie_toBinary_nid_postfix(trie, output.data(), actual - 1, &status) == actual && status == 15);
            Require(output.front() == 0x12345678);
            status = -128;
            Require(ucptrie_toBinary_nid_postfix(trie, output.data(), actual + 4, &status) == actual && status == -128);
            Require(std::memcmp(output.data(), binary.data(), actual) == 0 && output.back() == 0x12345678);
            status = 0;
            auto* reopened = ucptrie_openFromBinary_nid_postfix(type, width, output.data(), actual + 4, &actual, &status);
            Require(reopened && status == 0 && ucptrie_get_nid_postfix(reopened, firstSmall + 1) == 12);
            ucptrie_close_nid_postfix(reopened);
            if (type == 0) {
                const std::array<std::uint8_t, 4> utf8{0xf0, 0x90, 0x80, 0x81};
                Require(ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x81, utf8.data(), utf8.data() + 3) == (70001 << 3 | 3));
                Require(ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x80, utf8.data(), utf8.data()) == ((DataLength - 1) << 3));
                Require(ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x80, utf8.data(), utf8.data() + 2) == ((DataLength - 1) << 3 | 2));
            }
            status = 0;
            Require(ucptrie_openFromBinary_nid_postfix(1 - type, width, binary.data(), actual, nullptr, &status) == nullptr && status == 3);
            status = 0;
            Require(ucptrie_openFromBinary_nid_postfix(type, (width + 1) % 3, binary.data(), actual, nullptr, &status) == nullptr && status == 3);
            ucptrie_close_nid_postfix(trie);
        }
    }
}

void Errors() {
    auto binary = Binary(0, 0);
    const auto size = static_cast<std::int32_t>(binary.size() * 4);
    std::int32_t status = 7;
    std::int32_t actual = 123;
    Require(!ucptrie_openFromBinary_nid_postfix(-99, -99, nullptr, -1, &actual, &status) && status == 7 && actual == 123);
    Require(ucptrie_toBinary_nid_postfix(nullptr, nullptr, -1, &status) == 0 && status == 7);
    for (const int length : {-1, 0, 1, 15, 16, size - 1}) {
        status = 0;
        Require(!ucptrie_openFromBinary_nid_postfix(-1, -1, binary.data(), length, &actual, &status));
        Require(status == (length <= 0 ? 1 : 3) && actual == 123);
    }
    for (const int type : {-2, 2, 3}) {
        status = 0;
        Require(!ucptrie_openFromBinary_nid_postfix(type, -1, binary.data(), size, &actual, &status) && status == 1);
    }
    for (const int width : {-2, 3, 7}) {
        status = 0;
        Require(!ucptrie_openFromBinary_nid_postfix(-1, width, binary.data(), size, &actual, &status) && status == 1);
    }
    status = 0;
    Require(!ucptrie_openFromBinary_nid_postfix(-1, -1, reinterpret_cast<char*>(binary.data()) + 1, size, &actual, &status) && status == 1);
    for (const unsigned options : {3u, 7u, 8u, 0x38u, 0x80u, 0xc0u}) {
        auto invalid = binary;
        reinterpret_cast<std::uint16_t*>(invalid.data())[2] |= options;
        status = 0;
        Require(!ucptrie_openFromBinary_nid_postfix(-1, -1, invalid.data(), size, &actual, &status) && status == 3);
    }
    binary[0] = 0x33697254;
    status = 0;
    Require(!ucptrie_openFromBinary_nid_postfix(-1, -1, binary.data(), size, &actual, &status) && status == 3);
    binary[0] = 0x54726933;
    status = 0;
    auto* trie = ucptrie_openFromBinary_nid_postfix(-1, -1, binary.data(), size, nullptr, &status);
    Require(trie && status == 0);
    for (int capacity : {-1, 1}) {
        status = 0;
        Require(ucptrie_toBinary_nid_postfix(trie, nullptr, capacity, &status) == 0 && status == 1);
    }
    status = 0;
    Require(ucptrie_toBinary_nid_postfix(trie, reinterpret_cast<char*>(binary.data()) + 1, size, &status) == 0 && status == 1);
    Invalid([&] { ucptrie_openFromBinary_nid_postfix(-1, -1, binary.data(), size, nullptr, nullptr); });
    status = 0;
    Invalid([&] { ucptrie_openFromBinary_nid_postfix(-1, -1, nullptr, 16, nullptr, &status); });
    Invalid([&] { ucptrie_getType_nid_postfix(nullptr); });
    Invalid([&] { ucptrie_getValueWidth_nid_postfix(nullptr); });
    Invalid([&] { ucptrie_get_nid_postfix(nullptr, 0); });
    Invalid([&] { ucptrie_toBinary_nid_postfix(trie, nullptr, 0, nullptr); });
    status = 0;
    Invalid([&] { ucptrie_toBinary_nid_postfix(nullptr, nullptr, 0, &status); });
    ucptrie_close_nid_postfix(trie);
    ucptrie_close_nid_postfix(nullptr);
}

}

int main() {
    ValidBinaries();
    Errors();
    std::puts("ICU code point trie tests passed");
}
