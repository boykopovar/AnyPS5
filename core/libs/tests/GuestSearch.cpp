#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <source_location>
using Comparator = int (APS5_VABI *)(const void*, const void*);

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

    {
        std::array<Record, 6> table = records;
        std::size_t nel = table.size();
        calls = 0;
        int probe = 5;
        auto* hit = static_cast<Record*>(lfind_nid_postfix(&probe, table.data(), &nel, sizeof(Record), Compare));
        Require(hit && hit == table.data() + 2 && calls == 3);
        Require(nel == table.size());
        Require(std::strcmp(hit->payload, "five") == 0);
        probe = 4;
        calls = 0;
        Require(lfind_nid_postfix(&probe, table.data(), &nel, sizeof(Record), Compare) == nullptr);
        Require(calls == static_cast<int>(table.size()));
        std::size_t empty = 0;
        calls = 0;
        Require(lfind_nid_postfix(&probe, table.data(), &empty, sizeof(Record), Compare) == nullptr);
        Require(calls == 0);
        calls = 0;
        Require(lfind_nid_postfix(&probe, nullptr, &empty, sizeof(Record), Compare) == nullptr);
        Require(calls == 0);
    }

    {
        std::array<Record, 8> buffer{};
        for (std::size_t index = 0; index < records.size(); ++index) buffer[index] = records[index];
        const auto before_lsearch = buffer;
        std::size_t nel = records.size();
        Record present{5, "already"};
        calls = 0;
        auto* found = static_cast<Record*>(lsearch_nid_postfix(&present.key, buffer.data(), &nel, sizeof(Record), Compare));
        Require(found == buffer.data() + 2 && nel == records.size() && calls == 3);
        Require(std::memcmp(buffer.data(), before_lsearch.data(), sizeof(buffer)) == 0);
        Record fresh{4, "appended"};
        calls = 0;
        auto* appended = static_cast<Record*>(lsearch_nid_postfix(&fresh.key, buffer.data(), &nel, sizeof(Record), Compare));
        Require(appended == buffer.data() + records.size());
        Require(appended->key == 4 && nel == records.size() + 1);
        Require(calls == static_cast<int>(records.size()));
        Require(std::memcmp(appended, &fresh, sizeof(Record)) == 0);
        std::size_t zero = 0;
        Record first{42, "first"};
        calls = 0;
        auto* seeded = static_cast<Record*>(lsearch_nid_postfix(&first.key, buffer.data() + 7, &zero, sizeof(Record), Compare));
        Require(seeded == buffer.data() + 7 && zero == 1 && calls == 0);
        Require(std::memcmp(seeded, &first, sizeof(Record)) == 0);
    }

    {
        bool caught = false;
        std::size_t nel = 1;
        try { lfind_nid_postfix(nullptr, records.data(), &nel, sizeof(Record), Compare); }
        catch (const std::invalid_argument&) { caught = true; }
        Require(caught);
        caught = false;
        try { lsearch_nid_postfix(&key, nullptr, &nel, sizeof(Record), Compare); }
        catch (const std::invalid_argument&) { caught = true; }
        Require(caught);
        caught = false;
        try { lfind_nid_postfix(&key, records.data(), nullptr, sizeof(Record), Compare); }
        catch (const std::invalid_argument&) { caught = true; }
        Require(caught);
        std::size_t huge = std::numeric_limits<std::size_t>::max();
        calls = 0;
        caught = false;
        try { lfind_nid_postfix(&key, records.data(), &huge, sizeof(Record), Compare); }
        catch (const std::overflow_error&) { caught = true; }
        Require(caught && calls == 0 && huge == std::numeric_limits<std::size_t>::max());
        caught = false;
        Record scratch{};
        try { lsearch_nid_postfix(&key, &scratch, &huge, sizeof(Record), Compare); }
        catch (const std::overflow_error&) { caught = true; }
        Require(caught && calls == 0 && huge == std::numeric_limits<std::size_t>::max());
    }
}
