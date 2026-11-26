#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {

const auto GrowBy = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm_nid_postfix;
const auto Assign = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5__nid_postfix;
const auto Concatenate =
    _ZNSt3__1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9__nid_postfix;
constexpr auto MaximumSize = std::numeric_limits<std::uint64_t>::max() - 16u;

struct alignas(8) String {
    std::array<char, 24> bytes{};
};

std::uint64_t Field(const String& string, std::size_t offset) {
    std::uint64_t value;
    std::memcpy(&value, string.bytes.data() + offset, sizeof(value));
    return value;
}

char* Data(const String& string) {
    return reinterpret_cast<char*>(static_cast<std::uintptr_t>(Field(string, 16)));
}

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", failure);
    std::abort();
}

void RequireFreed(void* pointer) {
    try {
        static_cast<void>(ApplicationHeapUsableSize_nid_no_patch(pointer));
    } catch (const std::exception&) {
        return;
    }
    Require(false, "the previous guest allocation is still live");
}

std::string Pattern(std::size_t size, std::size_t seed) {
    std::string result(size, '\0');
    for (std::size_t i = 0; i < size; ++i) result[i] = i % 13 == 5 ? '\0' : static_cast<char>((i * 7 + seed) % 97 + 1);
    return result;
}

String Short(const std::string& contents) {
    String result;
    for (std::size_t i = 0; i < result.bytes.size(); ++i) result.bytes[i] = static_cast<char>(0x70 + i);
    result.bytes[0] = static_cast<char>(contents.size() << 1u);
    std::memcpy(result.bytes.data() + 1, contents.data(), contents.size());
    result.bytes[contents.size() + 1] = '\0';
    return result;
}

String Fields(std::uint64_t taggedCapacity, std::uint64_t size, std::uint64_t address) {
    String result;
    std::memcpy(result.bytes.data(), &taggedCapacity, sizeof(taggedCapacity));
    std::memcpy(result.bytes.data() + 8, &size, sizeof(size));
    std::memcpy(result.bytes.data() + 16, &address, sizeof(address));
    return result;
}

String Long(std::uint64_t allocation, const std::string& contents) {
    auto* data = static_cast<char*>(_Znwm_nid_postfix(allocation));
    std::memcpy(data, contents.c_str(), contents.size() + 1);
    return Fields(allocation | 1u, contents.size(), reinterpret_cast<std::uintptr_t>(data));
}

bool Holds(const String& string, std::uint64_t allocation, const std::string& contents) {
    const auto* data = Data(string);
    return Field(string, 0) == (allocation | 1u) && Field(string, 8) == contents.size() && data &&
           ApplicationHeapUsableSize_nid_no_patch(const_cast<char*>(data)) >= allocation &&
           std::memcmp(data, contents.c_str(), contents.size() + 1) == 0;
}

bool HoldsShort(const String& string, const std::string& contents) {
    return static_cast<unsigned char>(string.bytes[0]) == contents.size() << 1u &&
           std::memcmp(string.bytes.data() + 1, contents.c_str(), contents.size() + 1) == 0;
}

template<class Exception, class Call>
void RequireThrows(Call call, const String& string, const char* failure) {
    const auto before = string.bytes;
    try {
        call();
    } catch (const Exception&) {
        Require(string.bytes == before, failure);
        return;
    }
    Require(false, failure);
}

void TestGrowBy() {
    const std::string inlineContents{'a', 'b', '\0', 'c', 'd', 'e', 'f', 'g', 'h', 'i'};
    String small = Short(inlineContents);
    const auto sizeWord = Field(small, 8);
    GrowBy(&small, 22, 20, 10, 4, 2, 5);
    Require(Field(small, 0) == 49 && Field(small, 8) == sizeWord,
            "short growth must set the doubled capacity and leave the size word alone");
    Require(ApplicationHeapUsableSize_nid_no_patch(Data(small)) >= 48 &&
                std::memcmp(Data(small), inlineContents.data(), 4) == 0 &&
                std::memcmp(Data(small) + 9, inlineContents.data() + 6, 4) == 0,
            "short growth did not move the prefix and the tail past the inserted gap");
    _ZdlPv_nid_postfix(Data(small));

    const auto contents = Pattern(6150, 3);
    String large = Long(6160, contents);
    auto* previous = Data(large);
    GrowBy(&large, 6159, 1, 6150, 6000, 0, 1);
    Require(Field(large, 0) == 12321 && Field(large, 8) == 6150 && Data(large) != previous,
            "long growth must double the capacity when old_cap + delta_cap is smaller");
    Require(std::memcmp(Data(large), contents.data(), 6000) == 0 &&
                std::memcmp(Data(large) + 6001, contents.data() + 6000, 150) == 0,
            "long growth lost the block-copied prefix or tail");
    RequireFreed(previous);
    _ZdlPv_nid_postfix(Data(large));

    const auto shortContents = Pattern(30, 11);
    String grown = Long(32, shortContents);
    previous = Data(grown);
    GrowBy(&grown, 31, 100, 30, 10, 5, 50);
    Require(Field(grown, 0) == 145 && Field(grown, 8) == 30,
            "long growth must use old_cap + delta_cap when it exceeds twice the capacity");
    Require(std::memcmp(Data(grown), shortContents.data(), 10) == 0 &&
                std::memcmp(Data(grown) + 60, shortContents.data() + 15, 15) == 0,
            "long growth dropped the deleted range incorrectly");
    RequireFreed(previous);

    RequireThrows<std::length_error>([&] { GrowBy(&grown, 143, MaximumSize - 142, 30, 30, 0, 0); }, grown,
                                     "delta_cap past max_size() - old_cap must throw length_error");
    RequireThrows<std::bad_alloc>([&] { GrowBy(&grown, 143, MaximumSize - 143, 30, 30, 0, 0); }, grown,
                                  "the largest accepted delta_cap must reach the allocator");
    RequireThrows<std::invalid_argument>([&] { GrowBy(&grown, 31, 1, 30, 30, 0, 1); }, grown,
                                         "old_cap must match the string's capacity");
    RequireThrows<std::invalid_argument>([&] { GrowBy(&grown, 143, 1, 30, 20, 11, 1); }, grown,
                                         "n_copy + n_del past old_sz must be rejected");
    String shortString = Short(inlineContents);
    RequireThrows<std::invalid_argument>([&] { GrowBy(&shortString, 31, 1, 10, 10, 0, 1); }, shortString,
                                         "a short string has capacity 22");
    _ZdlPv_nid_postfix(Data(grown));

    const auto hugeCapacity = std::numeric_limits<std::uint64_t>::max() / 2;
    String buffer = Long(32, shortContents);
    String nearMaximum = Fields(hugeCapacity + 2u, 30, Field(buffer, 16));
    RequireThrows<std::bad_alloc>([&] { GrowBy(&nearMaximum, hugeCapacity, 1, 30, 30, 0, 0); }, nearMaximum,
                                  "old_cap at or past max_size() / 2 - 16 must grow to max_size() - 1");
    _ZdlPv_nid_postfix(Data(nearMaximum));
}

void TestAssign() {
    const std::string first{'x', '\0', 'y'};
    String target = Short(Pattern(7, 1));
    String source = Short(first);
    source.bytes[23] = 0x5a;
    Require(Assign(&target, &source) == &target && target.bytes == source.bytes,
            "short to short assignment must copy the whole representation");
    const auto unchanged = target.bytes;
    Require(Assign(&target, &target) == &target && target.bytes == unchanged, "self-assignment changed the string");

    const auto twenty = Pattern(20, 5);
    String longSource = Long(64, twenty);
    String inlineTarget = Short(Pattern(3, 9));
    const auto beforeInline = inlineTarget.bytes;
    Require(Assign(&inlineTarget, &longSource) == &inlineTarget && HoldsShort(inlineTarget, twenty) &&
                std::memcmp(inlineTarget.bytes.data() + 22, beforeInline.data() + 22, 2) == 0,
            "a short destination must stay short when the source fits");

    const auto forty = Pattern(40, 6);
    String fortySource = Long(48, forty);
    String promoted = Short(Pattern(3, 9));
    Require(Assign(&promoted, &fortySource) == &promoted && Holds(promoted, 48, forty),
            "a short destination must grow to __recommend(max(n, 44)) + 1");

    String reused = Long(64, Pattern(50, 2));
    auto* storage = Data(reused);
    Require(Assign(&reused, &source) == &reused && Data(reused) == storage && Holds(reused, 64, first),
            "a long destination must keep its allocation for a short source");
    const auto sixtyThree = Pattern(63, 4);
    String sixtyThreeSource = Long(64, sixtyThree);
    Require(Assign(&reused, &sixtyThreeSource) == &reused && Data(reused) == storage &&
                Holds(reused, 64, sixtyThree),
            "a long destination must reuse its allocation while n < its allocation");
    const auto beforeSelf = reused.bytes;
    Require(Assign(&reused, &reused) == &reused && reused.bytes == beforeSelf,
            "long self-assignment changed the string");

    const auto thirtyTwo = Pattern(32, 8);
    String thirtyTwoSource = Long(48, thirtyTwo);
    String narrow = Long(32, Pattern(10, 1));
    storage = Data(narrow);
    Require(Assign(&narrow, &thirtyTwoSource) == &narrow && Holds(narrow, 64, thirtyTwo),
            "n equal to the allocation must grow to twice the capacity");
    RequireFreed(storage);
    const auto big = Pattern(6150, 7);
    String bigSource = Long(6160, big);
    storage = Data(narrow);
    Require(Assign(&narrow, &bigSource) == &narrow && Holds(narrow, 6160, big),
            "a large source must grow to __recommend(n) + 1");
    RequireFreed(storage);

    String huge = Fields(std::numeric_limits<std::uint64_t>::max(), MaximumSize, 1);
    String tooLong = Short(first);
    RequireThrows<std::length_error>([&] { Assign(&tooLong, &huge); }, tooLong,
                                     "a source of max_size() characters must throw length_error");
    huge = Fields(std::numeric_limits<std::uint64_t>::max(), MaximumSize - 1, 1);
    RequireThrows<std::bad_alloc>([&] { Assign(&tooLong, &huge); }, tooLong,
                                  "a source below max_size() must reach the allocator");
    String longTarget = Long(32, Pattern(10, 1));
    huge = Fields(std::numeric_limits<std::uint64_t>::max(), MaximumSize, 1);
    RequireThrows<std::length_error>([&] { Assign(&longTarget, &huge); }, longTarget,
                                     "a long destination must throw length_error for max_size() characters");
    huge = Fields(std::numeric_limits<std::uint64_t>::max(), MaximumSize - 1, 1);
    RequireThrows<std::bad_alloc>([&] { Assign(&longTarget, &huge); }, longTarget,
                                  "a long destination must reach the allocator below max_size()");
    String invalidShort = Short(first);
    invalidShort.bytes[0] = static_cast<char>(23u << 1u);
    RequireThrows<std::invalid_argument>([&] { Assign(&longTarget, &invalidShort); }, longTarget,
                                         "a short source size of 23 must be rejected");
    RequireThrows<std::invalid_argument>([&] { Assign(&invalidShort, &source); }, invalidShort,
                                         "a short destination size of 23 must be rejected");
    String invalidLong = Fields(33, 32, Field(longTarget, 16));
    RequireThrows<std::invalid_argument>([&] { Assign(&longTarget, &invalidLong); }, longTarget,
                                         "a long source whose size reaches its allocation must be rejected");
    _ZdlPv_nid_postfix(Data(longTarget));

    for (auto* string : {&longSource, &fortySource, &promoted, &reused, &sixtyThreeSource, &thirtyTwoSource,
                         &narrow, &bigSource}) {
        _ZdlPv_nid_postfix(Data(*string));
    }
}

void TestConcatenate() {
    const std::string embedded{'x', '\0', 'y'};
    String right = Short(embedded);
    String result;
    Require(Concatenate(&result, "", &right) == &result && HoldsShort(result, embedded) &&
                std::all_of(result.bytes.begin() + 5, result.bytes.end(), [](char c) { return c == 0; }),
            "empty lhs must produce a zero-initialised short copy of rhs");

    String empty = Short("");
    Require(Concatenate(&result, "abc", &empty) == &result && HoldsShort(result, "abc"),
            "empty rhs must produce a copy of lhs");

    const std::string left(19, 'L');
    Require(Concatenate(&result, left.c_str(), &right) == &result && HoldsShort(result, left + embedded),
            "a 22-character result must stay short");
    String four = Short(std::string{'1', '\0', '3', '4'});
    Require(Concatenate(&result, left.c_str(), &four) == &result &&
                Holds(result, 32, left + std::string{'1', '\0', '3', '4'}),
            "a 23-character result must use the first long allocation");
    _ZdlPv_nid_postfix(Data(result));

    const auto thirty = Pattern(30, 12);
    String longRight = Long(32, thirty);
    const std::string prefix(5000, 'p');
    Require(Concatenate(&result, prefix.c_str(), &longRight) == &result && Holds(result, 5040, prefix + thirty),
            "a long result must hold lhs, then rhs with its embedded NULs");
    _ZdlPv_nid_postfix(Data(result));
    _ZdlPv_nid_postfix(Data(longRight));

    String huge = Fields(std::numeric_limits<std::uint64_t>::max(), MaximumSize - 2, 1);
    String untouched = Short("keep");
    RequireThrows<std::length_error>([&] { Concatenate(&untouched, "abc", &huge); }, untouched,
                                     "a result past max_size() must throw length_error");
    RequireThrows<std::bad_alloc>([&] { Concatenate(&untouched, "ab", &huge); }, untouched,
                                  "a result of max_size() characters must reach the allocator");
    RequireThrows<std::invalid_argument>([&] { Concatenate(&untouched, nullptr, &right); }, untouched,
                                         "a null lhs must be rejected");
    String invalidRight = Fields(33, 32, 1);
    RequireThrows<std::invalid_argument>([&] { Concatenate(&untouched, "ab", &invalidRight); }, untouched,
                                         "a long rhs whose size reaches its allocation must be rejected");
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the default guest heap");
    TestGrowBy();
    TestAssign();
    TestConcatenate();
    std::puts("libc++ string growth ABI checks passed");
    return 0;
}
