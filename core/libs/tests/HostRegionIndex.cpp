#include "prx/libc/include/HostRegionIndex.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using GuestArena::HostRegionIndex;
using Regions = std::vector<HostRegionIndex::Region>;

constexpr std::uintptr_t Domain = 40;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Regions RandomRegions(std::mt19937_64& random, std::uintptr_t origin, bool wellFormed) {
    Regions regions;
    const auto count = random() % 9;
    for (std::size_t index = 0; index < count; ++index) {
        std::uintptr_t start = random() % Domain;
        if (wellFormed && !regions.empty() && random() % 3 == 0) start = regions[random() % regions.size()].second - origin;
        std::uintptr_t end = random() % Domain;
        if (wellFormed) end = start + 1 + random() % 12;
        regions.emplace_back(origin + start, origin + end);
    }
    return regions;
}

void CompareAllQueries(const HostRegionIndex& index, const Regions& regions, std::uintptr_t origin) {
    for (std::uintptr_t from = 0; from < Domain + 3; ++from) {
        for (std::uintptr_t to = 0; to < Domain + 3; ++to) {
            const auto start = origin + from;
            const auto bytes = static_cast<std::size_t>((origin + to) - start);
            Require(index.Overlaps(regions, start, bytes) == HostRegionIndex::Linear(regions, start, bytes), "the index answers differently from the linear scan");
        }
    }
}

void CheckSmallDomains() {
    std::mt19937_64 random(0x1d3c);
    const std::uintptr_t top = std::numeric_limits<std::uintptr_t>::max() - 200;
    for (const std::uintptr_t origin : {std::uintptr_t{0}, std::uintptr_t{0x100000}, top}) {
        for (unsigned round = 0; round < 2500; ++round) {
            for (const bool wellFormed : {true, false}) {
                const auto regions = RandomRegions(random, origin, wellFormed);
                HostRegionIndex index;
                Require(!index.Active(), "an index that was never built is active");
                index.Build(regions);
                const bool degenerate = std::any_of(regions.begin(), regions.end(), [](const auto& region) { return region.first >= region.second; });
                Require(index.Active() == !degenerate, "the index is active over an empty or inverted region");
                CompareAllQueries(index, regions, origin);
            }
        }
    }
}

void CheckRebuild() {
    HostRegionIndex index;
    const Regions first{{10, 20}, {30, 40}};
    index.Build(first);
    Require(index.Overlaps(first, 15, 1) && !index.Overlaps(first, 20, 10) && index.Overlaps(first, 39, 5), "the first build answers wrongly");
    const Regions second{{50, 60}};
    index.Build(second);
    Require(!index.Overlaps(second, 15, 1) && index.Overlaps(second, 55, 1), "a rebuilt index kept the earlier regions");
    const Regions inverted{{50, 60}, {70, 70}};
    index.Build(inverted);
    Require(!index.Active() && index.Overlaps(inverted, 55, 1) == HostRegionIndex::Linear(inverted, 55, 1), "an index over an empty region stayed active");
    const Regions none;
    index.Build(none);
    Require(index.Active() && !index.Overlaps(none, 0, std::numeric_limits<std::size_t>::max()), "an empty region list overlaps");
}

void CheckTouchingBoundaries() {
    const Regions regions{{100, 200}, {200, 300}, {301, 400}};
    HostRegionIndex index;
    index.Build(regions);
    Require(index.Active(), "the index is inactive over well formed regions");
    Require(!index.Overlaps(regions, 200, 0) && index.Overlaps(regions, 150, 0) && !index.Overlaps(regions, 300, 1) && index.Overlaps(regions, 299, 2) && index.Overlaps(regions, 300, 2), "touching regions answer wrongly");
    Require(!index.Overlaps(regions, 300, 1) && index.Overlaps(regions, 301, 1) && !index.Overlaps(regions, 0, 100) && index.Overlaps(regions, 0, 101) && !index.Overlaps(regions, 400, 50), "region edges answer wrongly");
    const std::size_t wrap = std::size_t{150} - std::size_t{200};
    Require(!index.Overlaps(regions, 200, wrap) && index.Overlaps(regions, 150, std::size_t{50} - std::size_t{150}) == HostRegionIndex::Linear(regions, 150, std::size_t{50} - std::size_t{150}), "a wrapping range answers differently from the linear scan");
}

void CheckLargeLists() {
    std::mt19937_64 random(0x5ca1e);
    Regions regions;
    std::uintptr_t cursor = 0x10000;
    for (unsigned count = 0; count < 3000; ++count) {
        const auto gap = (random() % 4 == 0) ? 0 : random() % 0x4000000;
        const auto length = 1 + random() % 0x200000;
        regions.emplace_back(cursor + gap, cursor + gap + length);
        cursor += gap + length;
    }
    std::shuffle(regions.begin(), regions.end(), random);
    HostRegionIndex index;
    index.Build(regions);
    Require(index.Active(), "the index is inactive over a long region list");
    for (unsigned query = 0; query < 200000; ++query) {
        const auto& region = regions[random() % regions.size()];
        const std::uintptr_t edges[] = {region.first, region.second, region.first - 1, region.second + 1, random() % (cursor + 0x1000000)};
        const auto start = edges[random() % 5];
        const std::size_t sizes[] = {0, 1, 2, 0x1000, 0x10000, 0x4000000, random() % cursor, std::numeric_limits<std::size_t>::max() - start + 1 + random() % 8};
        const auto bytes = sizes[random() % 8];
        Require(index.Overlaps(regions, start, bytes) == HostRegionIndex::Linear(regions, start, bytes), "the index answers differently from the linear scan on a long list");
    }
}

}

int main() {
    try {
        CheckSmallDomains();
        CheckRebuild();
        CheckTouchingBoundaries();
        CheckLargeLists();
        std::cout << "host region index passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
