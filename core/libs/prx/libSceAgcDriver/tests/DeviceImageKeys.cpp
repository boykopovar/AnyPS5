#include "prx/libSceAgcDriver/Graphics/include/DeviceImageKeys.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

using namespace AgcDriver::Graphics;
namespace Abi = ShaderRecompiler::DeviceImageTableAbi;

namespace {

int failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

DeviceImageKeys::Key MakeKey(std::uint32_t seed) {
    DeviceImageKeys::Key key{};
    for (std::uint32_t word = 0; word < key.size(); ++word) key[word] = seed * 0x9e3779b9u + word * 0x7f4a7c15u + (seed >> 3u);
    return key;
}

std::uint32_t Home(const DeviceImageKeys::Key& key, std::uint32_t buckets) {
    return Abi::Hash(key) & (buckets - 1u);
}

std::vector<DeviceImageKeys::Key> SameHome(std::uint32_t buckets, std::size_t count) {
    std::vector<DeviceImageKeys::Key> keys;
    const auto home = Home(MakeKey(1u), buckets);
    for (std::uint32_t seed = 1u; keys.size() < count; ++seed) {
        const auto key = MakeKey(seed);
        if (Home(key, buckets) == home) keys.push_back(key);
    }
    return keys;
}

void testHash() {
    Expect(Abi::Hash(DeviceImageKeys::Key{}) == 0xfa15124cu, "the hash of the zero key is MurmurHash3_x86_32 of 60 zero bytes with seed 0x9747b28c");
    Expect(Abi::Hash(DeviceImageKeys::Key{1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u}) == 0x1c73aadcu, "the hash of 1..15 matches the reference");
    Expect(Abi::Hash(DeviceImageKeys::Key{0x00123400u, 0x01600000u, 0x8000c03fu, 0x9000fac0u, 0u, 0u, 0u, 0u, 0x89abcdefu, 0x01234567u, 5u, 0u, 0u, 0u, 0u}) == 0x4dbc4501u, "the hash of a T# key matches the reference");
    Expect(Abi::Hash(DeviceImageKeys::Key{0x00123400u, 0x01600000u, 0x8000c03fu, 0x9000fac0u, 0u, 0u, 0u, 0u, 0x89abcdefu, 0x01234567u, Abi::RuntimeSampler, 0x92u, 0x400000u, 0x500000u, 0u}) == 0xd28701c6u, "the hash of a T# and S# key matches the reference");
    Expect(Abi::Hash(DeviceImageKeys::Key{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}) == 0x1f3adfbdu, "the hash of all ones matches the reference");
    auto changed = MakeKey(7u);
    const auto original = Abi::Hash(changed);
    for (std::uint32_t word = 0; word < changed.size(); ++word) {
        auto other = changed;
        other[word] ^= 1u;
        Expect(Abi::Hash(other) != original, "a key differing in one bit of any word hashes differently");
    }
}

void testInsertFind() {
    DeviceImageKeys keys(64u, 64u);
    Expect(keys.Buckets() == 64u && keys.Entries() == 0u && keys.Words().size() == 64u * Abi::BucketWords, "a new table has its buckets and no entries");
    Expect(!keys.Find(MakeKey(1u)).has_value(), "an empty table finds nothing");
    for (std::uint32_t seed = 1; seed <= 20; ++seed) Expect(keys.Insert(MakeKey(seed), {seed, seed % 3u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a new key is inserted");
    Expect(keys.Entries() == 20u, "the table counts its entries");
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        const auto found = keys.Find(MakeKey(seed));
        Expect(found.has_value() && found->slot == seed && found->mode == seed % 3u && found->fault == 0u, "every inserted key is found with its value");
    }
    Expect(!keys.Find(MakeKey(21u)).has_value(), "a key never inserted is not found");
    Expect(keys.Insert(MakeKey(5u), {99u, 7u, 3u, 42u}) == DeviceImageKeys::Insertion::Updated && keys.Entries() == 20u, "inserting a present key rewrites its value without a new entry");
    Expect(keys.Find(MakeKey(5u)) == DeviceImageValue{99u, 7u, 3u, 42u}, "the rewritten value, sampler slot included, is found");
    std::size_t live = 0;
    keys.ForEach([&](const DeviceImageKeys::Key& key, const DeviceImageValue& value) {
        ++live;
        Expect(keys.Find(key) == value, "every visited entry is the one Find returns");
    });
    Expect(live == 20u, "ForEach visits every entry once");
}

void testLayout() {
    DeviceImageKeys keys(16u, 16u);
    const auto key = MakeKey(3u);
    Expect(keys.Insert(key, {11u, 2u, 5u, 9u}) == DeviceImageKeys::Insertion::Inserted, "the layout key is inserted");
    const auto bucket = Home(key, 16u);
    const auto* at = keys.Words().data() + bucket * Abi::BucketWords;
    Expect(std::equal(key.begin(), key.end(), at), "the key words lie at the start of the home bucket");
    Expect(at[Abi::StateWord] == Abi::LiveState && at[Abi::SlotWord] == 11u && at[Abi::ModeWord] == 2u && at[Abi::FaultWord] == 5u && at[Abi::SamplerSlotWord] == 9u, "the bucket holds the state, slot, mode, fault and sampler slot words");
    std::size_t nonzero = 0;
    for (std::size_t word = 0; word < keys.Words().size(); ++word) nonzero += (word / Abi::BucketWords != bucket && keys.Words()[word] != 0u) ? 1u : 0u;
    Expect(nonzero == 0u, "the other buckets stay zero");
}

void testCollisions() {
    DeviceImageKeys keys(64u, 64u);
    const auto colliding = SameHome(64u, 3u);
    for (std::uint32_t index = 0; index < colliding.size(); ++index) Expect(keys.Insert(colliding[index], {index + 1u, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "keys of one home bucket are all inserted");
    const auto home = Home(colliding[0], 64u);
    for (std::uint32_t index = 0; index < colliding.size(); ++index) {
        const auto* at = keys.Words().data() + ((home + index) & 63u) * Abi::BucketWords;
        Expect(std::equal(colliding[index].begin(), colliding[index].end(), at), "colliding keys take the buckets after their home in insertion order");
        Expect(keys.Find(colliding[index]) == DeviceImageValue{index + 1u, 0u, 0u}, "every colliding key is found");
    }
}

void testProbeLimit() {
    DeviceImageKeys bounded(64u, 64u);
    const auto colliding = SameHome(64u, Abi::ProbeSteps + 1u);
    for (std::uint32_t index = 0; index < Abi::ProbeSteps; ++index) Expect(bounded.Insert(colliding[index], {index, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "keys within the probe window of their home are inserted");
    Expect(bounded.Insert(colliding[Abi::ProbeSteps], {99u, 0u, 0u}) == DeviceImageKeys::Insertion::Full, "a key with no free bucket within the probe window of a table at its maximum is refused");
    Expect(!bounded.Find(colliding[Abi::ProbeSteps]).has_value(), "the refused key is not found");
    for (std::uint32_t index = 0; index < Abi::ProbeSteps; ++index) Expect(bounded.Find(colliding[index]) == DeviceImageValue{index, 0u, 0u}, "a refused insertion leaves the entries in place");

    DeviceImageKeys growing(64u, 1024u);
    for (std::uint32_t index = 0; index <= Abi::ProbeSteps; ++index) Expect(growing.Insert(colliding[index], {index, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a key past the probe window grows the table");
    Expect(growing.Buckets() > 64u && growing.Rebuilt(), "the table grew and reports the rebuild");
    for (std::uint32_t index = 0; index <= Abi::ProbeSteps; ++index) Expect(growing.Find(colliding[index]) == DeviceImageValue{index, 0u, 0u}, "every key is found after the growth");
}

void testLoadAndFull() {
    DeviceImageKeys keys(16u, 64u);
    std::uint32_t inserted = 0;
    for (std::uint32_t seed = 1; seed <= 200 && keys.Insert(MakeKey(seed), {seed, 0u, 0u}) != DeviceImageKeys::Insertion::Full; ++seed) ++inserted;
    Expect(keys.Buckets() == 64u, "the table grows up to its maximum");
    Expect(inserted <= 32u && keys.Entries() == inserted, "the table holds at most half its maximum buckets");
    for (std::uint32_t seed = 1; seed <= inserted; ++seed) Expect(keys.Find(MakeKey(seed)) == DeviceImageValue{seed, 0u, 0u}, "every entry of a full table is found");
    keys.Clear();
    Expect(keys.Entries() == 0u && keys.Rebuilt() && !keys.Find(MakeKey(1u)).has_value(), "a cleared table is empty and reports the rebuild");
    Expect(std::all_of(keys.Words().begin(), keys.Words().end(), [](std::uint32_t word) { return word == 0u; }), "a cleared table is all zeros");
    Expect(keys.Insert(MakeKey(1u), {1u, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a cleared table takes keys again");
}

void testChanged() {
    DeviceImageKeys keys(64u, 64u);
    static_cast<void>(keys.TakeChanged());
    Expect(!keys.Rebuilt(), "taking the changes clears the rebuild flag");
    const auto first = MakeKey(1u);
    const auto second = MakeKey(2u);
    static_cast<void>(keys.Insert(first, {1u, 0u, 0u}));
    static_cast<void>(keys.Insert(second, {2u, 0u, 0u}));
    static_cast<void>(keys.Insert(first, {3u, 0u, 0u}));
    auto changed = keys.TakeChanged();
    std::vector<std::uint32_t> expected{Home(first, 64u), Home(second, 64u)};
    if (expected[0] == expected[1]) expected[1] = (expected[1] + 1u) & 63u;
    std::ranges::sort(expected);
    Expect(changed == expected, "the changed buckets are reported once each, sorted");
    Expect(keys.TakeChanged().empty(), "the changes are taken once");
}

void testRing() {
    constexpr std::uint32_t capacity = 4u;
    std::vector<std::uint32_t> ring(Abi::RingRecordsWord + capacity * Abi::RecordWords, 0u);
    const auto write = [&](std::uint32_t record, const DeviceImageKeys::Key& key, std::uint32_t pc) {
        auto* at = ring.data() + Abi::RingRecordsWord + record * Abi::RecordWords;
        std::copy(key.begin(), key.end(), at);
        at[Abi::RecordPcWord] = pc;
    };
    write(0, MakeKey(1u), 0x40u);
    write(1, MakeKey(2u), 0x44u);
    write(2, MakeKey(1u), 0x48u);
    ring[Abi::RingCountWord] = 3u;
    const auto decoded = DecodeDeviceImageRing(ring, capacity);
    Expect(decoded.written == 3u && decoded.dropped == 0u && decoded.misses.size() == 3u, "a ring within its capacity decodes every record");
    Expect(decoded.misses[1].key == MakeKey(2u) && decoded.misses[1].pc == 0x44u, "a record carries its key and program counter");
    const auto distinct = DistinctDeviceImageMisses(decoded.misses);
    Expect(distinct.size() == 2u && distinct[0].key == MakeKey(1u) && distinct[0].pc == 0x40u && distinct[1].key == MakeKey(2u), "the drain keeps the first record of every key in ring order");
    write(3, MakeKey(3u), 0x4cu);
    ring[Abi::RingCountWord] = 9u;
    const auto overflowed = DecodeDeviceImageRing(ring, capacity);
    Expect(overflowed.written == 9u && overflowed.dropped == 5u && overflowed.misses.size() == capacity, "a count past the capacity reports the dropped records");
    bool refused = false;
    try {
        static_cast<void>(DecodeDeviceImageRing(std::span<const std::uint32_t>(ring).first(ring.size() - 1u), capacity));
    } catch (const std::exception&) {
        refused = true;
    }
    Expect(refused, "a ring buffer smaller than its capacity is refused");
}

void testConstruction() {
    bool refused = false;
    try {
        DeviceImageKeys keys(24u, 64u);
    } catch (const std::exception&) {
        refused = true;
    }
    Expect(refused, "a bucket count that is not a power of two is refused");
    refused = false;
    try {
        DeviceImageKeys keys(8u, 64u);
    } catch (const std::exception&) {
        refused = true;
    }
    Expect(refused, "a table smaller than the probe window is refused");
}

}

void testRetain() {
    DeviceImageKeys keys(64u, 64u);
    const auto colliding = SameHome(64u, 6u);
    for (std::uint32_t index = 0; index < colliding.size(); ++index) Expect(keys.Insert(colliding[index], {index + 1u, 0u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a colliding key is inserted");
    for (std::uint32_t seed = 5000; seed < 5010; ++seed) Expect(keys.Insert(MakeKey(seed), {seed, 1u, 0u}) == DeviceImageKeys::Insertion::Inserted, "a spread key is inserted");
    static_cast<void>(keys.TakeChanged());
    const auto dropped = keys.Retain([&](const DeviceImageKeys::Key& key) { return key != colliding[0] && key != colliding[2] && key != MakeKey(5004u); });
    Expect(dropped == 3u && keys.Entries() == 13u, "Retain drops the keys it is not asked to keep and counts the rest");
    Expect(!keys.Find(colliding[0]).has_value() && !keys.Find(colliding[2]).has_value() && !keys.Find(MakeKey(5004u)).has_value(), "a dropped key is no longer found");
    for (std::uint32_t index : {1u, 3u, 4u, 5u}) {
        const auto found = keys.Find(colliding[index]);
        Expect(found.has_value() && found->slot == index + 1u, "a kept key behind a dropped one in its probe sequence is still found with its value");
    }
    for (std::uint32_t seed = 5000; seed < 5010; ++seed) {
        if (seed == 5004u) continue;
        const auto found = keys.Find(MakeKey(seed));
        Expect(found.has_value() && found->slot == seed, "a kept spread key is found with its value");
    }
    Expect(keys.Rebuilt() && keys.TakeChanged().empty(), "Retain rebuilds the table, which uploads it whole");
    Expect(keys.Retain([](const DeviceImageKeys::Key&) { return true; }) == 0u && keys.Entries() == 13u, "keeping every key drops none");
}

int main() {
    testHash();
    testRetain();
    testInsertFind();
    testLayout();
    testCollisions();
    testProbeLimit();
    testLoadAndFull();
    testChanged();
    testRing();
    testConstruction();
    if (failures != 0) {
        std::fprintf(stderr, "%d device image key checks failed\n", failures);
        return 1;
    }
    std::printf("device image key tests passed\n");
    return 0;
}
