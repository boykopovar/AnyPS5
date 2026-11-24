#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
    USet* item = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(item, 0x41, 0x5a);
    auto* const unmapped = reinterpret_cast<std::int32_t*>(0x10);
    error = Success;
    errno = 71;
    Require(Rejects([&] { uset_getItem_nid_postfix(item, 0, unmapped, &end, nullptr, 0, &error); }), "an unwritable start is rejected");
    Require(Rejects([&] { uset_getItem_nid_postfix(item, 0, &start, unmapped, nullptr, 0, &error); }), "an unwritable end is rejected");
    Require(errno == 71 && error == Success, "unwritable item outputs keep errno and the status");
    Require(uset_getItem_nid_postfix(item, 0, &start, &end, nullptr, 0, &error) == 0 && start == 0x41 && end == 0x5a, "the range is still read");
    uset_close_nid_postfix(item);
}

void BinaryPropertySet() {
    constexpr std::int32_t alphabetic = 0;
    std::int32_t error = Success;
    const auto* set = u_getBinaryPropertySet_nid_postfix(alphabetic, &error);
    Require(set != nullptr && error == Success, "alphabetic property set");
    Require(u_getBinaryPropertySet_nid_postfix(alphabetic, &error) == set, "borrowed identity");
    Require(uset_contains_nid_postfix(set, 'A') && uset_contains_nid_postfix(set, 0x10400), "letter membership");
    Require(!uset_contains_nid_postfix(set, '1'), "nonletter is excluded");
    const auto ranges = Items(set);
    Require(std::ranges::find(ranges, std::pair{0x41, 0x5a}) != ranges.end(), "alphabetic range enumeration");
    auto* copy = uset_openEmpty_nid_postfix();
    Require(copy != nullptr, "mutable set allocation");
    uset_addAll_nid_postfix(copy, set);
    Require(Items(copy) == ranges, "copy borrowed ranges into a mutable set");
    uset_add_nid_postfix(copy, '1');
    Require(uset_contains_nid_postfix(copy, '1') && !uset_contains_nid_postfix(set, '1'), "copy is independent");
    uset_close_nid_postfix(copy);
    Require(u_getBinaryPropertySet_nid_postfix(-1, &error) == nullptr && error == IllegalArgument, "invalid property");
    Require(u_getBinaryPropertySet_nid_postfix(alphabetic, &error) == nullptr && error == IllegalArgument,
            "incoming error is preserved");
}

void DataFilePropertySets() {
    constexpr std::int32_t emoji = 57;
    constexpr std::int32_t extendedPictographic = 64;
    constexpr std::int32_t changesWhenNfkcCasefolded = 56;
    std::int32_t error = Success;
    const auto* set = u_getBinaryPropertySet_nid_postfix(emoji, &error);
    Require(set != nullptr && error == Success, "emoji property set");
    Require(uset_contains_nid_postfix(set, 0x1f600) && uset_contains_nid_postfix(set, 0x23), "emoji membership");
    Require(!uset_contains_nid_postfix(set, 'A'), "letter is not emoji");
    set = u_getBinaryPropertySet_nid_postfix(extendedPictographic, &error);
    Require(set != nullptr && error == Success && uset_contains_nid_postfix(set, 0xa9) && !uset_contains_nid_postfix(set, 0x23),
            "extended pictographic property set");
    set = u_getBinaryPropertySet_nid_postfix(changesWhenNfkcCasefolded, &error);
    Require(set != nullptr && error == Success && uset_contains_nid_postfix(set, 'A') && !uset_contains_nid_postfix(set, 'a'),
            "NFKC casefold property set");
}

void EmojiStringItems() {
    constexpr std::int32_t keycaps = 66;
    std::int32_t error = Success;
    const auto* set = u_getBinaryPropertySet_nid_postfix(keycaps, &error);
    Require(set && error == Success && uset_getItemCount_nid_postfix(set) == 12, "twelve emoji keycap sequences");
    Require(!uset_contains_nid_postfix(set, '#'), "a keycap component is not a member on its own");
    auto* copy = uset_openEmpty_nid_postfix();
    uset_addAll_nid_postfix(copy, set);
    uset_add_nid_postfix(copy, 'a');
    uset_addAll_nid_postfix(copy, set);
    Require(uset_getItemCount_nid_postfix(copy) == 13 && uset_getItemCount_nid_postfix(set) == 12, "union keeps distinct strings and ranges");
    std::int32_t start = 7, end = 8;
    std::array<char16_t, 4> output{u'?', u'?', u'?', u'?'};
    Require(uset_getItem_nid_postfix(copy, 1, &start, &end, output.data(), 2, &error) == 3 && error == BufferOverflow,
            "short buffer reports the entire sequence length");
    Require(output[0] == u'?' && start == 7 && end == 8, "a short string read leaves the outputs intact");
    error = Success;
    Require(uset_getItem_nid_postfix(copy, 1, nullptr, nullptr, output.data(), 3, &error) == 3 && error == -124,
            "exact capacity gives the not-terminated warning");
    Require(output == std::array<char16_t, 4>{u'#', 0xfe0f, 0x20e3, u'?'}, "string item follows ranges and preserves UTF-16 units");
    Require(uset_getItem_nid_postfix(copy, 1, nullptr, nullptr, output.data(), 4, &error) == 3 && error == Success && output[3] == 0,
            "space for a terminator clears the warning");
    uset_close_nid_postfix(copy);
}

template<class F>
void RequiresRuntimeErrorKeepingErrno(F call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void BinaryPropertySetGuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr, "allocate status page");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED, "allocate status page");
#endif
    const std::int32_t failing = IllegalArgument;
    std::memset(page, 0, pageSize);
    std::memcpy(static_cast<char*>(page) + 16, &failing, sizeof(failing));
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect status page");
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0, "protect status page");
#endif
    auto* readOnlyZero = static_cast<std::int32_t*>(page);
    auto* readOnlyFailing = reinterpret_cast<std::int32_t*>(static_cast<char*>(page) + 16);
    auto* unmapped = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    constexpr std::int32_t alphabetic = 0;

    RequiresRuntimeErrorKeepingErrno([&] { u_getBinaryPropertySet_nid_postfix(alphabetic, unmapped); },
                                     "unreadable error throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_getBinaryPropertySet_nid_postfix(-1, readOnlyZero); },
                                     "unwritable error throws and keeps errno");
    Require(u_getBinaryPropertySet_nid_postfix(-1, readOnlyFailing) == nullptr, "read-only incoming error returns immediately");
    Require(u_getBinaryPropertySet_nid_postfix(alphabetic, readOnlyZero) != nullptr, "success does not write the error");

#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
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

void AdditionalOperations() {
    static_assert(std::is_same_v<decltype(&uset_clear_nid_postfix), void (APS5_VABI*)(USet*)>);
    static_assert(std::is_same_v<decltype(&uset_compact_nid_postfix), void (APS5_VABI*)(USet*)>);
    static_assert(std::is_same_v<decltype(&uset_complement_nid_postfix), void (APS5_VABI*)(USet*)>);
    static_assert(std::is_same_v<decltype(&uset_complementRange_nid_postfix), void (APS5_VABI*)(USet*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_containsRange_nid_postfix), std::int8_t (APS5_VABI*)(const USet*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_equals_nid_postfix), std::int8_t (APS5_VABI*)(const USet*, const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_isEmpty_nid_postfix), std::int8_t (APS5_VABI*)(const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_remove_nid_postfix), void (APS5_VABI*)(USet*, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_removeRange_nid_postfix), void (APS5_VABI*)(USet*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_retain_nid_postfix), void (APS5_VABI*)(USet*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_size_nid_postfix), std::int32_t (APS5_VABI*)(const USet*)>);

    USet* set = uset_openEmpty_nid_postfix();
    USet* equal = uset_openEmpty_nid_postfix();
    Require(uset_isEmpty_nid_postfix(set) && uset_size_nid_postfix(set) == 0, "new set is empty");
    uset_addRange_nid_postfix(set, 10, 20);
    uset_addRange_nid_postfix(set, 30, 40);
    uset_addAll_nid_postfix(equal, set);
    Require(uset_size_nid_postfix(set) == 22, "set size counts code points");
    Require(uset_containsRange_nid_postfix(set, 12, 19) && !uset_containsRange_nid_postfix(set, 15, 25), "range containment");
    Require(uset_equals_nid_postfix(set, equal), "equal sets");
    uset_compact_nid_postfix(set);
    Require(uset_equals_nid_postfix(set, equal), "compaction preserves contents");
    uset_remove_nid_postfix(set, 15);
    Require(!uset_contains_nid_postfix(set, 15) && !uset_equals_nid_postfix(set, equal), "remove code point");
    uset_removeRange_nid_postfix(set, 18, 35);
    Require(Items(set) == Ranges{{10, 14}, {16, 17}, {36, 40}}, "remove range");
    uset_retain_nid_postfix(set, 16, 37);
    Require(Items(set) == Ranges{{16, 17}, {36, 37}}, "retain range");
    uset_complementRange_nid_postfix(set, 17, 36);
    Require(Items(set) == Ranges{{16, 16}, {18, 35}, {37, 37}}, "complement range");
    uset_complement_nid_postfix(set);
    Require(!uset_contains_nid_postfix(set, 16) && uset_contains_nid_postfix(set, 17) &&
            !uset_contains_nid_postfix(set, 18) && !uset_contains_nid_postfix(set, 37) &&
            uset_contains_nid_postfix(set, 38), "whole set complement");
    Require(uset_size_nid_postfix(set) == CodePointLimit - 20, "complement size");
    uset_clear_nid_postfix(set);
    Require(uset_isEmpty_nid_postfix(set) && uset_size_nid_postfix(set) == 0, "clear set");
    uset_close_nid_postfix(equal);
    uset_close_nid_postfix(set);
}

void AdditionalQueries() {
    static_assert(std::is_same_v<decltype(&uset_indexOf_nid_postfix), std::int32_t (APS5_VABI*)(const USet*, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_charAt_nid_postfix), std::int32_t (APS5_VABI*)(const USet*, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_getStringCount_nid_postfix), std::int32_t (APS5_VABI*)(const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_containsAll_nid_postfix), std::int8_t (APS5_VABI*)(const USet*, const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_containsNone_nid_postfix), std::int8_t (APS5_VABI*)(const USet*, const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_containsSome_nid_postfix), std::int8_t (APS5_VABI*)(const USet*, const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_hasStrings_nid_postfix), std::int8_t (APS5_VABI*)(const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_isFrozen_nid_postfix), std::int8_t (APS5_VABI*)(const USet*)>);
    static_assert(std::is_same_v<decltype(&uset_span_nid_postfix), std::int32_t (APS5_VABI*)(const USet*, const char16_t*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_spanBack_nid_postfix), std::int32_t (APS5_VABI*)(const USet*, const char16_t*, std::int32_t, std::int32_t)>);
    static_assert(std::is_same_v<decltype(&uset_spanUTF8_nid_postfix), std::int32_t (APS5_VABI*)(const USet*, const char*, std::int32_t, std::int32_t)>);

    USet* set = uset_openEmpty_nid_postfix();
    USet* subset = uset_openEmpty_nid_postfix();
    USet* other = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(set, 10, 20);
    uset_addRange_nid_postfix(set, 30, 40);
    uset_addRange_nid_postfix(subset, 12, 18);
    uset_addRange_nid_postfix(other, 50, 60);
    Require(uset_indexOf_nid_postfix(set, 10) == 0 && uset_indexOf_nid_postfix(set, 20) == 10 &&
            uset_indexOf_nid_postfix(set, 30) == 11 && uset_indexOf_nid_postfix(set, 25) == -1, "set indexes");
    Require(uset_charAt_nid_postfix(set, 0) == 10 && uset_charAt_nid_postfix(set, 10) == 20 &&
            uset_charAt_nid_postfix(set, 11) == 30 && uset_charAt_nid_postfix(set, 22) == -1, "characters at indexes");
    Require(uset_containsAll_nid_postfix(set, subset) && !uset_containsAll_nid_postfix(subset, set), "contains all");
    Require(uset_containsNone_nid_postfix(set, other) && !uset_containsSome_nid_postfix(set, other), "disjoint sets");
    uset_add_nid_postfix(other, 35);
    Require(!uset_containsNone_nid_postfix(set, other) && uset_containsSome_nid_postfix(set, other), "intersecting sets");
    Require(uset_getStringCount_nid_postfix(set) == 0 && !uset_hasStrings_nid_postfix(set) && !uset_isFrozen_nid_postfix(set),
            "mutable code point set queries");
    std::int32_t error = Success;
    const auto* strings = u_getBinaryPropertySet_nid_postfix(66, &error);
    Require(strings && error == Success && uset_getStringCount_nid_postfix(strings) == 12 &&
            uset_hasStrings_nid_postfix(strings) && uset_isFrozen_nid_postfix(strings), "frozen string set queries");
    USet* letters = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(letters, 'a', 'z');
    Require(uset_span_nid_postfix(letters, u"abc1xyz", -1, 1) == 3 &&
            uset_span_nid_postfix(letters, u"123a", 4, 0) == 3, "forward spans");
    Require(uset_spanBack_nid_postfix(letters, u"abc123", 6, 0) == 3 &&
            uset_spanBack_nid_postfix(letters, u"123abc", 6, 1) == 3, "backward spans");
    Require(uset_spanUTF8_nid_postfix(letters, "abc1xyz", -1, 1) == 3 &&
            uset_spanUTF8_nid_postfix(letters, "123a", 4, 0) == 3, "UTF-8 spans");
    uset_close_nid_postfix(letters);
    uset_close_nid_postfix(other);
    uset_close_nid_postfix(subset);
    uset_close_nid_postfix(set);
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
    Require(Rejects([] { uset_clear_nid_postfix(nullptr); }), "null set clear");
    Require(Rejects([] { uset_compact_nid_postfix(nullptr); }), "null set compact");
    Require(Rejects([] { uset_complement_nid_postfix(nullptr); }), "null set complement");
    Require(Rejects([] { uset_complementRange_nid_postfix(nullptr, 1, 2); }), "null set complement range");
    Require(Rejects([] { uset_containsRange_nid_postfix(nullptr, 1, 2); }), "null set contains range");
    Require(Rejects([&] { uset_equals_nid_postfix(set, nullptr); }), "null set equals");
    Require(Rejects([] { uset_isEmpty_nid_postfix(nullptr); }), "null set empty query");
    Require(Rejects([] { uset_remove_nid_postfix(nullptr, 1); }), "null set remove");
    Require(Rejects([] { uset_removeRange_nid_postfix(nullptr, 1, 2); }), "null set remove range");
    Require(Rejects([] { uset_retain_nid_postfix(nullptr, 1, 2); }), "null set retain");
    Require(Rejects([] { uset_size_nid_postfix(nullptr); }), "null set size");
    Require(Rejects([] { uset_indexOf_nid_postfix(nullptr, 1); }), "null set index");
    Require(Rejects([] { uset_charAt_nid_postfix(nullptr, 0); }), "null set character");
    Require(Rejects([] { uset_getStringCount_nid_postfix(nullptr); }), "null set string count");
    Require(Rejects([&] { uset_containsAll_nid_postfix(set, nullptr); }), "null set contains all");
    Require(Rejects([&] { uset_containsNone_nid_postfix(set, nullptr); }), "null set contains none");
    Require(Rejects([&] { uset_containsSome_nid_postfix(set, nullptr); }), "null set contains some");
    Require(Rejects([] { uset_hasStrings_nid_postfix(nullptr); }), "null set strings query");
    Require(Rejects([] { uset_isFrozen_nid_postfix(nullptr); }), "null set frozen query");
    Require(Rejects([&] { uset_span_nid_postfix(set, nullptr, 1, 1); }), "null span text");
    Require(Rejects([&] { uset_spanBack_nid_postfix(set, nullptr, 1, 1); }), "null backward span text");
    Require(Rejects([&] { uset_spanUTF8_nid_postfix(set, nullptr, 1, 1); }), "null UTF-8 span text");
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
    BinaryPropertySet();
    DataFilePropertySets();
    EmojiStringItems();
    BinaryPropertySetGuestPointers();
    SingleCodePoints();
    CodePointRanges();
    Union();
    AdditionalOperations();
    AdditionalQueries();
    ItemAccess();
    InvalidSets();
    Random();
    return 0;
}
