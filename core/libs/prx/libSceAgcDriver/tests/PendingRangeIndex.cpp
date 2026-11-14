#include "prx/libSceAgcDriver/Graphics/include/PendingRangeIndex.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct Range {
    std::uint64_t begin;
    std::uint64_t end;
};

bool Overlaps(const Range& range, std::uint64_t address, std::size_t bytes) {
    return bytes != 0 && address < range.end && range.begin < address + bytes;
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void RandomAgainstScan() {
    std::mt19937_64 random(0x5eed);
    for (int round = 0; round < 2000; ++round) {
        const auto count = static_cast<std::size_t>(random() % 40);
        std::vector<Range> ranges;
        AgcDriver::Graphics::PendingRangeIndex<std::size_t> index;
        for (std::size_t item = 0; item < count; ++item) {
            const auto begin = (random() % 4096) * 64;
            const auto bytes = 1 + random() % (random() % 4 == 0 ? 65536 : 4096);
            ranges.push_back({begin, begin + bytes});
            index.Add(begin, begin + bytes, item, item);
        }
        index.Seal();
        for (int query = 0; query < 200; ++query) {
            const auto address = (random() % 4200) * 64 + random() % 64;
            const auto bytes = static_cast<std::size_t>(random() % 8 == 0 ? 0 : random() % 20000);
            const auto except = static_cast<std::size_t>(random() % (count + 1));
            bool scanned = false;
            std::size_t firstScanned = count;
            std::vector<bool> scannedSet(count, false);
            for (std::size_t item = 0; item < count; ++item) {
                if (!Overlaps(ranges[item], address, bytes)) continue;
                scannedSet[item] = true;
                if (item == except) continue;
                scanned = true;
                if (firstScanned == count) firstScanned = item;
            }
            const bool indexed = index.AnyOverlapping(address, bytes, [&](std::size_t item) { return item != except; });
            Require(indexed == scanned, "the index and the scan disagree on whether a range overlaps");
            std::vector<bool> visited(count, false);
            index.ForEachOverlapping(address, bytes, [&](std::size_t item) {
                Require(!visited[item], "the index visited an item twice");
                visited[item] = true;
            });
            Require(visited == scannedSet, "the index visited a different set of overlapping items than the scan");
            const auto* first = index.FirstByPosition(address, bytes, [&](std::size_t item) { return item != except; });
            Require((first == nullptr) == (firstScanned == count), "the index and the scan disagree on the first overlapping item");
            Require(first == nullptr || first->item == firstScanned, "the index named a later item than the first overlapping one");
        }
    }
}

void InsertEraseAgainstScan() {
    std::mt19937_64 random(0xfeed);
    struct Live {
        std::uint64_t begin;
        std::uint64_t end;
        int item;
    };
    AgcDriver::Graphics::PendingRangeIndex<int> index;
    std::vector<Live> list;
    int nextItem = 0;
    std::size_t order = 0;
    for (int step = 0; step < 20000; ++step) {
        if (list.empty() || random() % 3 != 0) {
            const auto begin = (random() % 256) * 4096;
            const auto end = begin + 1 + random() % (random() % 5 == 0 ? 262144 : 16384);
            list.push_back({begin, end, nextItem});
            index.Insert(begin, end, nextItem++, order++);
        } else {
            const auto at = static_cast<std::size_t>(random() % list.size());
            Require(index.Erase(list[at].begin, list[at].item), "the index lost an entry it was given");
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(at));
        }
        Require(index.Size() == list.size(), "the index holds a different number of entries than the list");
        Require(!index.Erase(1u << 30, -1), "the index erased an entry it was never given");
        for (int query = 0; query < 8; ++query) {
            const auto address = (random() % 300) * 4096 + random() % 4096;
            const auto bytes = static_cast<std::size_t>(random() % 70000);
            int firstScanned = -1;
            std::size_t overlapping = 0;
            for (const auto& live : list) {
                if (!Overlaps({live.begin, live.end}, address, bytes)) continue;
                ++overlapping;
                if (firstScanned < 0 && live.item % 2 == 0) firstScanned = live.item;
            }
            std::size_t visited = 0;
            index.ForEachOverlapping(address, bytes, [&](int) { ++visited; });
            Require(visited == overlapping, "after inserts and erases the index visited a different number of overlapping entries than the scan");
            const auto* first = index.FirstByPosition(address, bytes, [](int item) { return item % 2 == 0; });
            Require((first == nullptr ? -1 : first->item) == firstScanned, "after inserts and erases the index named a different first match than the list order");
        }
        const auto& entries = index.Entries();
        for (std::size_t at = 1; at < entries.size(); ++at) Require(entries[at - 1].begin <= entries[at].begin, "the index entries are out of base order");
    }
}

void Edges() {
    AgcDriver::Graphics::PendingRangeIndex<int> index;
    index.Add(100, 200, 1, 0);
    index.Add(0, 1000, 2, 1);
    index.Add(300, 301, 3, 2);
    index.Seal();
    const auto any = [&](std::uint64_t address, std::size_t bytes, int skip) { return index.AnyOverlapping(address, bytes, [&](int item) { return item != skip; }); };
    Require(any(500, 1, 0), "a long earlier range covering the query was missed");
    Require(!any(500, 1, 2), "an excluded item was reported");
    Require(any(200, 101, 2), "a one-byte range at the end of the query was missed");
    Require(!any(200, 100, 2), "a range touching the query's end was reported as overlapping");
    Require(!any(1000, 10, -1), "a range ending at the query's start was reported as overlapping");
    Require(!any(150, 0, -1), "an empty query overlapped");
    Require(any(~std::uint64_t{0} - 5, 100, -1) == false, "a query past the address space overlapped");
}

}

int main() {
    try {
        Edges();
        RandomAgainstScan();
        InsertEraseAgainstScan();
        std::cout << "pending range index tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pending range index test failed: " << error.what() << '\n';
        return 1;
    }
}
