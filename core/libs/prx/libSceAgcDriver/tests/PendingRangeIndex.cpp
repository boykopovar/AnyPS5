#include "prx/libSceAgcDriver/Graphics/include/PendingRangeIndex.hpp"
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
        std::cout << "pending range index tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pending range index test failed: " << error.what() << '\n';
        return 1;
    }
}
