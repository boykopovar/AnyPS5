#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

using Comparator = int (APS5_VABI *)(const void*, const void*);
extern "C" void* APS5_VABI bsearch_nid_postfix(const void*, const void*, std::size_t, std::size_t, Comparator);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct Record {
    int key;
    char payload[13];
};

using Records = std::array<Record, 6>;

thread_local int comparisons = 0;

int APS5_VABI Compare(const void* key, const void* record) {
    ++comparisons;
    const int left = *static_cast<const int*>(key);
    const int right = static_cast<const Record*>(record)->key;
    return (left > right) - (left < right);
}

Records MakeRecords() {
    return {{{1, "one"}, {3, "three"}, {5, "five"}, {5, "duplicate"}, {7, "seven"}, {9, "nine"}}};
}

const Record* Search(int key, const Record* records, std::size_t count) {
    comparisons = 0;
    return static_cast<const Record*>(bsearch_nid_postfix(&key, records, count, sizeof(Record), Compare));
}

const Case presentKeys{"Bsearch_PresentKey_ReturnsMatchingRecordWithinThreeComparisons", [] {
    const Records records = MakeRecords();
    for (int key = 1; key <= 9; key += 2) {
        const std::string input = "key " + std::to_string(key);
        const Record* found = Search(key, records.data(), records.size());
        Require(comparisons <= 3, input + " comparisons: " + std::to_string(comparisons));
        Require(found != nullptr, input + " found");
        Require(found >= records.data() && found < records.data() + records.size(), input + " result inside the array");
        RequireEqual(found->key, key, input + " record key");
    }
}};

const Case absentKeys{"Bsearch_AbsentKey_ReturnsNullWithinThreeComparisons", [] {
    const Records records = MakeRecords();
    for (int key = 0; key <= 10; key += 2) {
        const std::string input = "key " + std::to_string(key);
        const Record* found = Search(key, records.data(), records.size());
        Require(comparisons <= 3, input + " comparisons: " + std::to_string(comparisons));
        Require(found == nullptr, input + " not found");
    }
}};

const Case unmodified{"Bsearch_AnyKey_LeavesArrayUnmodified", [] {
    const Records records = MakeRecords();
    std::array<unsigned char, sizeof(Records)> before{};
    std::memcpy(before.data(), records.data(), sizeof(Records));
    for (int key = 0; key <= 10; ++key) Search(key, records.data(), records.size());
    Require(std::memcmp(before.data(), records.data(), sizeof(Records)) == 0, "array bytes unchanged");
}};

const Case emptyArray{"Bsearch_EmptyNullArray_ReturnsNullWithoutComparing", [] {
    Require(Search(1, nullptr, 0) == nullptr, "result");
    RequireEqual(comparisons, 0, "comparisons");
}};

const Case singleMatch{"Bsearch_SingleMatchingElement_ReturnsIt", [] {
    const Records records = MakeRecords();
    Require(Search(1, records.data(), 1) == records.data(), "result");
}};

const Case singleMismatch{"Bsearch_SingleNonMatchingElement_ReturnsNull", [] {
    const Records records = MakeRecords();
    Require(Search(2, records.data(), 1) == nullptr, "result");
}};

const Case overflow{"Bsearch_CountOverflowingAddressSpace_ThrowsOverflowError", [] {
    const Records records = MakeRecords();
    Testing::RequireThrows<std::overflow_error>(
        [&] { Search(2, records.data(), std::numeric_limits<std::size_t>::max()); }, "maximum count");
}};

} // namespace
