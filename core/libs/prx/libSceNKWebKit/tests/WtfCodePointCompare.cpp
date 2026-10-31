#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

unsigned checks = 0;

void Check(bool condition, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "WTF code unit comparison failed at line %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

struct BorrowedString {
    StringImpl impl;
    String value;

    explicit BorrowedString(StringView view)
        : impl{2, view.length, view.data, view.is8Bit ? 4U : 0U}, value(&impl) {}

    ~BorrowedString() {
        value.impl = nullptr;
    }
};

void Pair(StringView first, StringView second, int expected) {
    Require(_ZN3WTF16codePointCompareENS_10StringViewES0_(first, second) == expected);
    BorrowedString a(first);
    BorrowedString b(second);
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(a.value, b.value) == expected);
    Require(a.impl.refCount == 2 && b.impl.refCount == 2);
    Require(a.impl.data == first.data && b.impl.data == second.data);
    Require(a.impl.length == first.length && b.impl.length == second.length);
    Require(a.impl.hashAndFlags == (first.is8Bit ? 4U : 0U));
    Require(b.impl.hashAndFlags == (second.is8Bit ? 4U : 0U));
}

int Reference(const std::vector<char16_t>& first, const std::vector<char16_t>& second) {
    if (std::lexicographical_compare(first.begin(), first.end(), second.begin(), second.end())) return -1;
    return std::lexicographical_compare(second.begin(), second.end(), first.begin(), first.end()) ? 1 : 0;
}

void Widths(const std::vector<char16_t>& first, const std::vector<char16_t>& second) {
    const int expected = Reference(first, second);
    const StringView a{first.data(), static_cast<unsigned>(first.size()), false};
    const StringView b{second.data(), static_cast<unsigned>(second.size()), false};
    Pair(a, b, expected);
    const bool firstNarrow = std::all_of(first.begin(), first.end(), [](char16_t unit) { return unit < 256; });
    const bool secondNarrow = std::all_of(second.begin(), second.end(), [](char16_t unit) { return unit < 256; });
    const std::vector<unsigned char> narrowFirst(first.begin(), first.end());
    const std::vector<unsigned char> narrowSecond(second.begin(), second.end());
    const StringView na{narrowFirst.data(), static_cast<unsigned>(narrowFirst.size()), true};
    const StringView nb{narrowSecond.data(), static_cast<unsigned>(narrowSecond.size()), true};
    if (firstNarrow) Pair(na, b, expected);
    if (secondNarrow) Pair(a, nb, expected);
    if (firstNarrow && secondNarrow) Pair(na, nb, expected);
}

void Boundaries() {
    const std::vector<std::vector<char16_t>> strings{
        {}, {0}, {0, 0}, {0, 1}, {1}, {0x7f}, {0x80}, {0xff}, {0x100},
        {'a'}, {'a', 0}, {'a', 0, 'b'}, {'a', 0, 'c'}, {'a', 'b'}, {'a', 'b', 'c'},
        {0xd7ff}, {0xd800}, {0xdbff}, {0xdc00}, {0xdfff}, {0xe000}, {0xffff},
        {0xd800, 0xdc00}, {0xdbff, 0xdfff}, {0xd800, 0}, {0xdc00, 0xd800}
    };
    for (const auto& first : strings) {
        for (const auto& second : strings) Widths(first, second);
    }
    for (unsigned first = 0; first < 256; ++first) {
        for (unsigned second = 0; second < 256; ++second) {
            Widths({static_cast<char16_t>(first)}, {static_cast<char16_t>(second)});
        }
    }
    constexpr std::array<char16_t, 11> edges{0, 0x7f, 0x80, 0xff, 0x100, 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xe000, 0xffff};
    for (unsigned first = 0; first <= 0xffff; ++first) {
        for (char16_t second : edges) {
            const char16_t a = static_cast<char16_t>(first);
            Pair({&a, 1, false}, {&second, 1, false}, (a > second) - (a < second));
        }
    }
}

void RandomSequences() {
    std::mt19937 random(0x50415053);
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        std::vector<char16_t> first(random() % 96);
        std::vector<char16_t> second(random() % 96);
        const unsigned mask = iteration % 2 ? 0xff : 0xffff;
        for (auto& unit : first) unit = static_cast<char16_t>(random() & mask);
        for (auto& unit : second) unit = static_cast<char16_t>(random() & mask);
        if (iteration % 3 == 0) {
            const auto length = std::min(first.size(), second.size());
            std::copy_n(first.begin(), length, second.begin());
            if (length && iteration % 2) second[length - 1] ^= 1;
        }
        Widths(first, second);
        Widths(second, first);
    }
}

void NullAndLength() {
    String nullString;
    BorrowedString empty({nullptr, 0, false});
    const unsigned char byte = 0;
    BorrowedString nonempty({&byte, 1, true});
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(nullString, nullString) == 0);
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(nullString, empty.value) == 0);
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(empty.value, nullString) == 0);
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(nullString, nonempty.value) == -1);
    Require(_ZN3WTF16codePointCompareERKNS_6StringES2_(nonempty.value, nullString) == 1);
    Pair({nullptr, 0, false}, {nullptr, 0, true}, 0);
    Pair({nullptr, 0, true}, {nullptr, std::numeric_limits<unsigned>::max(), false}, -1);
    Pair({nullptr, std::numeric_limits<unsigned>::max(), false}, {nullptr, 0, false}, 1);
    for (bool narrow : {false, true}) {
        for (bool reverse : {false, true}) {
            StringView missing{nullptr, 1, narrow};
            const char16_t unit = 1;
            StringView valid{&unit, 1, false};
            if (reverse) std::swap(missing, valid);
            bool threw = false;
            try {
                _ZN3WTF16codePointCompareENS_10StringViewES0_(missing, valid);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw);
            BorrowedString a(missing);
            BorrowedString b(valid);
            threw = false;
            try {
                _ZN3WTF16codePointCompareERKNS_6StringES2_(a.value, b.value);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw);
        }
    }
}

}

int main() {
    Boundaries();
    RandomSequences();
    NullAndLength();
    std::printf("WTF code unit comparison: %u checks passed\n", checks);
}
