#include "prx/libSceAgcDriver/Graphics/include/RangeIndex.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::RangeIndex;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Entry {
    std::uint64_t begin;
    std::uint64_t end;
    int id;
    std::uint64_t sequence;
};

class BruteForce {
public:
    void Insert(int id, std::uint64_t begin, std::uint64_t end) { entries.push_back({begin, end, id, ++sequence}); }
    std::size_t Remove(int id, bool all) {
        std::size_t removed = 0;
        for (auto it = entries.begin(); it != entries.end();) {
            if (it->id == id && (all || removed == 0)) {
                it = entries.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }
    std::vector<int> Overlapping(std::uint64_t address, std::uint64_t bytes) const {
        std::vector<Entry> hits;
        for (const auto& entry : entries) {
            if (bytes != 0 && address < entry.end && entry.begin < address + bytes) hits.push_back(entry);
        }
        return order(std::move(hits));
    }
    std::vector<int> StartingAt(std::uint64_t begin) const {
        std::vector<Entry> hits;
        for (const auto& entry : entries) {
            if (entry.begin == begin) hits.push_back(entry);
        }
        return order(std::move(hits));
    }
    std::size_t Size() const { return entries.size(); }
    const std::vector<Entry>& All() const { return entries; }

private:
    static std::vector<int> order(std::vector<Entry> hits) {
        std::sort(hits.begin(), hits.end(), [](const Entry& a, const Entry& b) { return a.begin != b.begin ? a.begin < b.begin : a.sequence < b.sequence; });
        std::vector<int> ids;
        for (const auto& hit : hits) ids.push_back(hit.id);
        return ids;
    }
    std::vector<Entry> entries;
    std::uint64_t sequence = 0;
};

std::vector<int> Collect(RangeIndex<int>& index, std::uint64_t address, std::uint64_t bytes) {
    std::vector<int> ids;
    index.Overlapping(address, bytes, [&](int id) {
        ids.push_back(id);
        return false;
    });
    return ids;
}

std::vector<int> CollectStarting(const RangeIndex<int>& index, std::uint64_t begin) {
    std::vector<int> ids;
    index.StartingAt(begin, [&](int id) {
        ids.push_back(id);
        return false;
    });
    return ids;
}

void CheckSmall() {
    RangeIndex<int> index;
    Require(Collect(index, 0, 100).empty(), "an empty index returned an entry");
    index.Insert(1, 100, 200);
    index.Insert(2, 150, 160);
    index.Insert(3, 100, 120);
    index.Insert(4, 300, 300);
    Require((Collect(index, 0, 100) == std::vector<int>{}), "a range ending where the first entry starts overlapped it");
    Require((Collect(index, 199, 1) == std::vector<int>{1}), "the last byte of an entry was missed");
    Require((Collect(index, 200, 1) == std::vector<int>{}), "the byte after an entry overlapped it");
    Require((Collect(index, 110, 100) == std::vector<int>{1, 3, 2}), "overlap order is not by start, then registration");
    Require((Collect(index, 100, 0).empty()), "an empty query range returned entries");
    Require((Collect(index, 250, 100) == std::vector<int>{4}) && Collect(index, 300, 10).empty(), "a zero-length entry does not follow the strict overlap rule of the scan");
    Require((CollectStarting(index, 100) == std::vector<int>{1, 3}), "entries at one start are not in registration order");
    int seen = 0;
    const bool stopped = index.Overlapping(0, 1000, [&](int) { return ++seen == 2; });
    Require(stopped && seen == 2, "the walk did not stop when asked");
    Require(index.Remove(1, 100) == 1, "a present entry was not removed");
    Require(index.Remove(1, 100) == 0, "an absent entry was removed");
    Require((CollectStarting(index, 100) == std::vector<int>{3}), "a removal changed the order of its neighbours");
    index.Insert(1, 100, 200);
    Require((CollectStarting(index, 100) == std::vector<int>{3, 1}), "a re-registered entry did not go behind the others");
    const std::uint64_t top = ~std::uint64_t{0};
    index.Insert(5, top - 10, top);
    Require((Collect(index, top - 5, 100) == std::vector<int>{5}), "a query running past the address space missed an entry");
}

void CheckRandom(std::uint32_t seed, std::uint64_t space, std::uint64_t maxLength, int rounds) {
    std::mt19937_64 random(seed);
    RangeIndex<int> index;
    BruteForce expected;
    int nextId = 0;
    std::vector<int> live;
    for (int round = 0; round < rounds; ++round) {
        const auto action = random() % 10;
        if (action < 5 || live.empty()) {
            const auto begin = random() % space;
            const auto end = begin + random() % (maxLength + 1);
            const int id = nextId++;
            index.Insert(id, begin, end);
            expected.Insert(id, begin, end);
            live.push_back(id);
        } else if (action < 8) {
            const auto at = random() % live.size();
            const int id = live[at];
            std::uint64_t begin = 0;
            for (const auto& entry : expected.All()) {
                if (entry.id == id) begin = entry.begin;
            }
            Require(index.Remove(id, begin) == expected.Remove(id, false), "a removal went differently from the list");
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
        } else {
            index.Insert(nextId, 7, 7 + maxLength);
            expected.Insert(nextId, 7, 7 + maxLength);
            live.push_back(nextId++);
        }
        Require(index.Size() == expected.Size(), "the index and the list differ in size");
        for (int query = 0; query < 4; ++query) {
            const auto address = random() % (space + maxLength);
            const auto bytes = random() % (maxLength + 2);
            Require(Collect(index, address, bytes) == expected.Overlapping(address, bytes), "overlap query " + std::to_string(address) + "+" + std::to_string(bytes) + " differs from the scan at round " + std::to_string(round));
        }
        const auto start = random() % space;
        Require(CollectStarting(index, start) == expected.StartingAt(start), "the entries at one start differ from the scan");
        if (!expected.All().empty()) {
            const auto& pick = expected.All()[random() % expected.All().size()];
            Require(CollectStarting(index, pick.begin) == expected.StartingAt(pick.begin), "the entries at an existing start differ from the scan");
        }
    }
    while (!live.empty()) {
        const int id = live.back();
        live.pop_back();
        std::uint64_t begin = 0;
        for (const auto& entry : expected.All()) {
            if (entry.id == id) begin = entry.begin;
        }
        Require(index.Remove(id, begin, true) == expected.Remove(id, true), "the final removal differs from the list");
    }
    Require(index.Size() == 0 && Collect(index, 0, space + maxLength).empty(), "entries are left after removing every one");
}

}

int main() {
    try {
        CheckSmall();
        CheckRandom(1, 64, 8, 4000);
        CheckRandom(2, 4096, 512, 4000);
        CheckRandom(3, 1ull << 40, 1ull << 30, 3000);
        CheckRandom(4, 16, 64, 3000);
        std::cout << "range index tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
