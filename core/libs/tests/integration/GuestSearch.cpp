#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <source_location>
using Comparator = int (APS5_VABI *)(const void*, const void*);
extern "C" void* APS5_VABI bsearch_nid_postfix(const void*, const void*, std::size_t, std::size_t, Comparator);
static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) { std::fprintf(stderr, "Guest search failed at line %u\n", location.line()); std::abort(); }
}
struct Record { int key; char payload[13]; };
static int calls = 0;
static int APS5_VABI Compare(const void* key, const void* record) {
    ++calls;
    const int left = *static_cast<const int*>(key);
    const int right = static_cast<const Record*>(record)->key;
    return (left > right) - (left < right);
}
int main() {
    const std::array<Record, 6> records{{{1, "one"}, {3, "three"}, {5, "five"},
        {5, "duplicate"}, {7, "seven"}, {9, "nine"}}};
    std::array<unsigned char, sizeof(records)> before{};
    std::memcpy(before.data(), records.data(), sizeof(records));
    for (int key = 0; key <= 10; ++key) {
        calls = 0;
        const auto* found = static_cast<Record*>(bsearch_nid_postfix(&key, records.data(), records.size(), sizeof(Record), Compare));
        Require(calls <= 3);
        if (key % 2) Require(found && found->key == key && found >= records.data() && found < records.data() + records.size());
        else Require(!found);
    }
    Require(std::memcmp(before.data(), records.data(), sizeof(records)) == 0);
    calls = 0;
    int key = 1;
    Require(bsearch_nid_postfix(&key, nullptr, 0, sizeof(Record), Compare) == nullptr && calls == 0);
    Require(bsearch_nid_postfix(&key, records.data(), 1, sizeof(Record), Compare) == records.data());
    key = 2;
    Require(!bsearch_nid_postfix(&key, records.data(), 1, sizeof(Record), Compare));
    bool rejected = false;
    try { bsearch_nid_postfix(&key, records.data(), std::numeric_limits<std::size_t>::max(), sizeof(Record), Compare); }
    catch (const std::overflow_error&) { rejected = true; }
    Require(rejected);
}
