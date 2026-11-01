#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t CodePointLimit = 0x110000;

using Ranges = std::vector<std::pair<std::int32_t, std::int32_t>>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU set: %s\n", message);
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Ranges Items(const USet* set) {
    Ranges ranges;
    const std::int32_t count = uset_getItemCount_nid_postfix(set);
    for (std::int32_t index = 0; index < count; ++index) {
        std::int32_t start = -7, end = -7, error = Success;
        Require(uset_getItem_nid_postfix(set, index, &start, &end, nullptr, 0, &error) == 0 && error == Success, "range item");
        ranges.emplace_back(start, end);
    }
    return ranges;
}

void Empty() {
    USet* set = uset_openEmpty_nid_postfix();
    Require(set && uset_getItemCount_nid_postfix(set) == 0, "empty set");
    for (const std::int32_t codePoint : {0, 0x41, 0x10ffff, -1, CodePointLimit, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
        Require(uset_contains_nid_postfix(set, codePoint) == 0, "empty set contains nothing");
    std::int32_t start = 3, end = 4, error = Success;
    Require(uset_getItem_nid_postfix(set, 0, &start, &end, nullptr, 0, &error) == -1 && error == IndexOutOfBounds && start == 3 && end == 4, "no items");
    uset_close_nid_postfix(set);
}

void SingleCodePoints() {
    USet* set = uset_openEmpty_nid_postfix();
    uset_add_nid_postfix(set, 0x61);
    uset_add_nid_postfix(set, 0x63);
    Require(Items(set) == Ranges{{0x61, 0x61}, {0x63, 0x63}}, "separate code points");
    uset_add_nid_postfix(set, 0x62);
    Require(Items(set) == Ranges{{0x61, 0x63}}, "adjacent code points merge");
    uset_add_nid_postfix(set, 0x62);
    uset_add_nid_postfix(set, 0x60);
    uset_add_nid_postfix(set, 0x64);
    Require(Items(set) == Ranges{{0x60, 0x64}}, "extending both ends");
    uset_add_nid_postfix(set, -5);
    uset_add_nid_postfix(set, 0x200000);
    Require(Items(set) == Ranges{{0, 0}, {0x60, 0x64}, {0x10ffff, 0x10ffff}}, "out-of-range code points are pinned");
    Require(uset_contains_nid_postfix(set, 0) == 1 && uset_contains_nid_postfix(set, 0x10ffff) == 1, "pinned code points are members");
    Require(uset_contains_nid_postfix(set, -5) == 0 && uset_contains_nid_postfix(set, 0x200000) == 0, "contains is not pinned");
    uset_close_nid_postfix(set);
}

void CodePointRanges() {
    USet* set = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(set, 5, 3);
    Require(uset_getItemCount_nid_postfix(set) == 0, "reversed range adds nothing");
    uset_addRange_nid_postfix(set, 0x41, 0x41);
    Require(Items(set) == Ranges{{0x41, 0x41}}, "equal endpoints add that code point");
    uset_addRange_nid_postfix(set, 0x41, 0x41);
    Require(Items(set) == Ranges{{0x41, 0x41}}, "adding the same code point again changes nothing");
    USet* fresh = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(fresh, 0x110000, 0x110005);
    uset_addRange_nid_postfix(fresh, -9, -9);
    Require(Items(fresh) == Ranges{{0, 0}, {0x10ffff, 0x10ffff}}, "out-of-range equal endpoints pin and add");
    uset_close_nid_postfix(fresh);
    uset_close_nid_postfix(set);
    set = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(set, -5, -1);
    Require(Items(set) == Ranges{{0, 0}}, "a range below zero pins to U+0000");
    uset_addRange_nid_postfix(set, 0x200000, 0x300000);
    Require(Items(set) == Ranges{{0, 0}, {0x10ffff, 0x10ffff}}, "a range above U+10FFFF pins to it");
    uset_addRange_nid_postfix(set, 10, 20);
    uset_addRange_nid_postfix(set, 15, 30);
    uset_addRange_nid_postfix(set, 40, 50);
    uset_addRange_nid_postfix(set, 51, 60);
    Require(Items(set) == Ranges{{0, 0}, {10, 30}, {40, 60}, {0x10ffff, 0x10ffff}}, "overlapping and adjacent ranges merge");
    uset_addRange_nid_postfix(set, 31, 39);
    Require(Items(set) == Ranges{{0, 0}, {10, 60}, {0x10ffff, 0x10ffff}}, "a gap is filled");
    uset_addRange_nid_postfix(set, 0x10fffe, std::numeric_limits<std::int32_t>::max());
    uset_addRange_nid_postfix(set, std::numeric_limits<std::int32_t>::min(), 1);
    Require(Items(set) == Ranges{{0, 1}, {10, 60}, {0x10fffe, 0x10ffff}}, "pinned range ends");
    uset_addRange_nid_postfix(set, 2, 0x10fffd);
    Require(Items(set) == Ranges{{0, 0x10ffff}}, "all code points");
    Require(uset_contains_nid_postfix(set, 0x10ffff) == 1 && uset_contains_nid_postfix(set, CodePointLimit) == 0 && uset_contains_nid_postfix(set, -1) == 0, "full set bounds");
    uset_close_nid_postfix(set);
}

void Union() {
    USet* first = uset_openEmpty_nid_postfix();
    USet* second = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(first, 0x30, 0x39);
    uset_addRange_nid_postfix(first, 0x100, 0x1ff);
    uset_addRange_nid_postfix(second, 0x3a, 0x40);
    uset_addRange_nid_postfix(second, 0x150, 0x250);
    uset_add_nid_postfix(second, 0x1f600);
    uset_addAll_nid_postfix(first, second);
    Require(Items(first) == Ranges{{0x30, 0x40}, {0x100, 0x250}, {0x1f600, 0x1f600}}, "union");
    Require(Items(second) == Ranges{{0x3a, 0x40}, {0x150, 0x250}, {0x1f600, 0x1f600}}, "the added set is unchanged");
    uset_addAll_nid_postfix(first, first);
    Require(Items(first) == Ranges{{0x30, 0x40}, {0x100, 0x250}, {0x1f600, 0x1f600}}, "union with itself");
    USet* empty = uset_openEmpty_nid_postfix();
    uset_addAll_nid_postfix(empty, first);
    Require(Items(empty) == Items(first), "union into an empty set");
    uset_close_nid_postfix(empty);
    uset_close_nid_postfix(second);
    uset_close_nid_postfix(first);
}

void ItemAccess() {
    USet* set = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(set, 0x41, 0x5a);
    std::int32_t start = 9, end = 9, error = Success;
    std::array<char16_t, 4> text{0x78, 0x78, 0x78, 0x78};
    Require(uset_getItem_nid_postfix(set, 0, &start, &end, text.data(), 4, &error) == 0 && start == 0x41 && end == 0x5a && text[0] == 0x78, "range item leaves the string buffer alone");
    Require(uset_getItem_nid_postfix(set, -1, &start, &end, nullptr, 0, &error) == -1 && error == IllegalArgument, "negative index");
    error = BufferOverflow;
    start = 1;
    Require(uset_getItem_nid_postfix(set, 0, &start, &end, nullptr, 0, &error) == 0 && error == BufferOverflow && start == 1, "a set error returns 0");
    error = Success;
    Require(uset_getItem_nid_postfix(set, 1, &start, &end, nullptr, 0, &error) == -1 && error == IndexOutOfBounds, "index past the ranges");
    error = Success;
    Require(uset_getItem_nid_postfix(nullptr, -1, &start, &end, nullptr, 0, &error) == -1 && error == IllegalArgument, "a negative index is checked before the set");
    error = Success;
    Require(Rejects([&] { uset_getItem_nid_postfix(set, 0, nullptr, &end, nullptr, 0, &error); }), "null range start");
    Require(Rejects([&] { uset_getItem_nid_postfix(set, 0, &start, nullptr, nullptr, 0, &error); }), "null range end");
    Require(Rejects([&] { uset_getItem_nid_postfix(set, 0, &start, &end, nullptr, 0, nullptr); }), "null error pointer");
    uset_close_nid_postfix(set);
}

void InvalidSets() {
    alignas(8) std::array<std::uint8_t, 64> foreign{};
    auto* forged = reinterpret_cast<USet*>(foreign.data());
    USet* set = uset_openEmpty_nid_postfix();
    Require(Rejects([] { uset_add_nid_postfix(nullptr, 1); }), "null set add");
    Require(Rejects([] { uset_contains_nid_postfix(nullptr, 1); }), "null set contains");
    Require(Rejects([] { uset_getItemCount_nid_postfix(nullptr); }), "null set count");
    Require(Rejects([&] { uset_addAll_nid_postfix(set, nullptr); }), "null added set");
    Require(Rejects([&] { uset_addRange_nid_postfix(forged, 1, 2); }), "foreign memory");
    Require(Rejects([&] { uset_close_nid_postfix(forged); }), "closing foreign memory");
    uset_close_nid_postfix(nullptr);
    uset_close_nid_postfix(set);
}

void Random() {
    std::vector<bool> expected(CodePointLimit);
    USet* set = uset_openEmpty_nid_postfix();
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    const auto next = [&state] {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::int32_t>(state >> 33);
    };
    for (int step = 0; step < 400; ++step) {
        const std::int32_t start = next() % (CodePointLimit + 64) - 32;
        const std::int32_t length = next() % (step % 4 == 0 ? 4096 : 64);
        if (step % 3 == 0) {
            uset_add_nid_postfix(set, start);
            expected[static_cast<std::size_t>(std::clamp(start, 0, CodePointLimit - 1))] = true;
            continue;
        }
        const std::int32_t end = start + length - 16;
        uset_addRange_nid_postfix(set, start, end);
        const std::int32_t low = std::clamp(start, 0, CodePointLimit - 1);
        const std::int32_t high = std::clamp(end, 0, CodePointLimit - 1);
        for (std::int32_t codePoint = low; codePoint <= high; ++codePoint) expected[static_cast<std::size_t>(codePoint)] = true;
    }
    Ranges runs;
    for (std::int32_t codePoint = 0; codePoint < CodePointLimit; ++codePoint) {
        const bool member = expected[static_cast<std::size_t>(codePoint)];
        Require(uset_contains_nid_postfix(set, codePoint) == (member ? 1 : 0), "random contains");
        if (!member) continue;
        if (!runs.empty() && runs.back().second == codePoint - 1) runs.back().second = codePoint;
        else runs.emplace_back(codePoint, codePoint);
    }
    Require(Items(set) == runs, "random ranges are maximal runs");
    uset_close_nid_postfix(set);
}

}

int main() {
    Empty();
    SingleCodePoints();
    CodePointRanges();
    Union();
    ItemAccess();
    InvalidSets();
    Random();
    return 0;
}
