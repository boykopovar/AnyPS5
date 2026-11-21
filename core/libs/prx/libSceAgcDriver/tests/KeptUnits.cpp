#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::StampUnkeptUnits;
using AgcDriver::Graphics::UnitLayout;
using Kept = std::map<std::uint64_t, std::uint64_t>;

constexpr std::uint64_t Untouched = 0xdead;
constexpr std::uint64_t Stamp = 0x5151;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t ReferenceBegin(const UnitLayout& layout, std::uint32_t unit) {
    return layout.base + static_cast<std::uint64_t>(unit) * layout.unitBytes;
}

std::uint64_t ReferenceBytes(const UnitLayout& layout, std::uint32_t unit) {
    return std::min<std::uint64_t>(layout.unitBytes, layout.totalBytes - static_cast<std::uint64_t>(unit) * layout.unitBytes);
}

bool ReferenceKeptForCpu(const Kept& kept, const UnitLayout& layout, const std::vector<std::uint64_t>& generations, std::uint32_t unit) {
    const auto begin = ReferenceBegin(layout, unit);
    const auto end = begin + ReferenceBytes(layout, unit);
    for (auto it = kept.lower_bound(begin); it != kept.end() && it->first < end; ++it) {
        if (it->second == generations[unit]) return true;
    }
    return false;
}

void ReferenceStamp(const Kept& kept, const UnitLayout& layout, const std::vector<std::uint64_t>& generations, std::vector<std::uint64_t>& stamps, std::uint32_t first, std::uint32_t count) {
    for (std::uint32_t unit = first; unit < first + count; ++unit) {
        if (!ReferenceKeptForCpu(kept, layout, generations, unit)) stamps[unit] = Stamp;
    }
}

std::vector<std::uint64_t> Walk(const Kept& kept, const UnitLayout& layout, const std::vector<std::uint64_t>& generations, std::uint32_t first, std::uint32_t count) {
    std::vector<std::uint64_t> stamps(generations.size(), Untouched);
    StampUnkeptUnits(kept, layout, generations, stamps, first, count, Stamp);
    return stamps;
}

std::vector<std::uint64_t> Reference(const Kept& kept, const UnitLayout& layout, const std::vector<std::uint64_t>& generations, std::uint32_t first, std::uint32_t count) {
    std::vector<std::uint64_t> stamps(generations.size(), Untouched);
    ReferenceStamp(kept, layout, generations, stamps, first, count);
    return stamps;
}

std::string Describe(const UnitLayout& layout, std::uint32_t first, std::uint32_t count, const std::string& label) {
    return label + " (base " + std::to_string(layout.base) + ", unit " + std::to_string(layout.unitBytes) + ", total " + std::to_string(layout.totalBytes) + ", first " + std::to_string(first) + ", count " + std::to_string(count) + ")";
}

void Compare(const Kept& kept, const UnitLayout& layout, const std::vector<std::uint64_t>& generations, std::uint32_t first, std::uint32_t count, const std::string& label) {
    const auto expected = Reference(kept, layout, generations, first, count);
    const auto actual = Walk(kept, layout, generations, first, count);
    for (std::size_t unit = 0; unit < expected.size(); ++unit) {
        Require(expected[unit] == actual[unit], Describe(layout, first, count, label) + ": unit " + std::to_string(unit) + " differs from the per-unit answer");
    }
}

constexpr std::uint64_t Block = 65536;
constexpr std::uint64_t Base = 0x400000000;

UnitLayout Blocks(std::uint32_t units, std::uint64_t tail = 0) {
    return {Base, Block, units * Block - (Block - (tail == 0 ? Block : tail))};
}

std::vector<std::uint64_t> Generations(std::uint32_t units, std::uint64_t value) {
    return std::vector<std::uint64_t>(units, value);
}

void CheckEmptyMap() {
    const auto layout = Blocks(8);
    const auto generations = Generations(8, 3);
    const auto stamps = Walk({}, layout, generations, 2, 4);
    for (std::uint32_t unit = 0; unit < 8; ++unit) {
        Require(stamps[unit] == (unit >= 2 && unit < 6 ? Stamp : Untouched), "an empty map must stamp exactly the requested units");
    }
    Compare({}, layout, generations, 0, 8, "empty map");
}

void CheckZeroCount() {
    const auto layout = Blocks(4);
    const Kept kept{{Base, 1}};
    const auto stamps = Walk(kept, layout, Generations(4, 1), 2, 0);
    for (const auto value : stamps) Require(value == Untouched, "a zero count must stamp nothing");
}

void CheckKeptInsideOneUnit() {
    const auto layout = Blocks(6);
    const auto generations = Generations(6, 7);
    const Kept kept{{Base + 2 * Block + 100, 7}};
    const auto stamps = Walk(kept, layout, generations, 0, 6);
    for (std::uint32_t unit = 0; unit < 6; ++unit) {
        Require(stamps[unit] == (unit == 2 ? Untouched : Stamp), "a matching entry must keep exactly its own unit");
    }
    Compare(kept, layout, generations, 0, 6, "one kept unit");
}

void CheckBoundaries() {
    const auto layout = Blocks(5);
    const auto generations = Generations(5, 9);
    const Kept atBegin{{Base + Block, 9}};
    const Kept atLastByte{{Base + 2 * Block - 1, 9}};
    const Kept atNextBegin{{Base + 2 * Block, 9}};
    const Kept beforeBase{{Base - 1, 9}};
    const Kept atEnd{{Base + 5 * Block, 9}};
    for (const auto& kept : {atBegin, atLastByte, atNextBegin, beforeBase, atEnd}) {
        for (std::uint32_t first = 0; first < 5; ++first) {
            for (std::uint32_t count = 0; first + count <= 5; ++count) Compare(kept, layout, generations, first, count, "entry on a unit boundary");
        }
    }
    const auto lastByte = Walk(atLastByte, layout, generations, 0, 5);
    Require(lastByte[1] == Untouched && lastByte[2] == Stamp, "an entry on the last byte of a unit belongs to that unit");
    const auto nextBegin = Walk(atNextBegin, layout, generations, 0, 5);
    Require(nextBegin[1] == Stamp && nextBegin[2] == Untouched, "an entry on the first byte of a unit belongs to that unit");
}

void CheckRangeStartsInsideAndBeforeKeptRanges() {
    const auto layout = Blocks(10);
    auto generations = Generations(10, 4);
    const Kept kept{{Base + 1 * Block, 4}, {Base + 3 * Block + 16, 4}, {Base + 4 * Block, 4}, {Base + 7 * Block + 8, 4}};
    for (std::uint32_t first = 0; first < 10; ++first) {
        for (std::uint32_t count = 0; first + count <= 10; ++count) Compare(kept, layout, generations, first, count, "range over kept ranges");
    }
    generations[3] = 5;
    generations[7] = 5;
    for (std::uint32_t first = 0; first < 10; ++first) {
        for (std::uint32_t count = 0; first + count <= 10; ++count) Compare(kept, layout, generations, first, count, "range over kept ranges with other generations");
    }
}

void CheckAdjacentAndOverlappingEntries() {
    const auto layout = Blocks(4);
    auto generations = Generations(4, 6);
    Kept kept;
    for (std::uint64_t at = Base; at < Base + 4 * Block; at += 4096) kept.emplace(at, 6 + (at / 4096) % 3);
    for (std::uint32_t first = 0; first < 4; ++first) {
        for (std::uint32_t count = 0; first + count <= 4; ++count) Compare(kept, layout, generations, first, count, "dense entries");
    }
    generations = {6, 7, 8, 9};
    for (std::uint32_t first = 0; first < 4; ++first) {
        for (std::uint32_t count = 0; first + count <= 4; ++count) Compare(kept, layout, generations, first, count, "dense entries and per-unit generations");
    }
    const Kept twoInOne{{Base + Block + 1, 99}, {Base + Block + 2, 6}, {Base + Block + 3, 98}};
    const auto stamps = Walk(twoInOne, layout, Generations(4, 6), 0, 4);
    Require(stamps[1] == Untouched, "a matching entry between two others must keep the unit");
}

void CheckLastUnitIsShort() {
    const auto layout = Blocks(5, 4096);
    Require(layout.totalBytes == 4 * Block + 4096, "the test layout must have a short last unit");
    const auto generations = Generations(5, 2);
    const Kept inTail{{Base + 4 * Block + 4000, 2}};
    const Kept pastTail{{Base + 4 * Block + 4096, 2}};
    const Kept beyond{{Base + 5 * Block, 2}};
    for (const auto& kept : {inTail, pastTail, beyond}) {
        for (std::uint32_t first = 0; first < 5; ++first) {
            for (std::uint32_t count = 0; first + count <= 5; ++count) Compare(kept, layout, generations, first, count, "short last unit");
        }
    }
    const auto past = Walk(pastTail, layout, generations, 0, 5);
    Require(past[4] == Stamp, "an entry past the last byte of the image belongs to no unit");
    const auto tail = Walk(inTail, layout, generations, 0, 5);
    Require(tail[4] == Untouched, "an entry in the short last unit keeps it");
}

void CheckSingleUnitImage() {
    const UnitLayout layout{Base, 123456, 123456};
    const auto generations = Generations(1, 11);
    for (const Kept& kept : {Kept{}, Kept{{Base, 11}}, Kept{{Base + 123455, 11}}, Kept{{Base + 123456, 11}}, Kept{{Base, 12}}}) {
        Compare(kept, layout, generations, 0, 1, "single unit");
    }
}

void CheckZeroSizedUnits() {
    const UnitLayout layout{Base, 0, 0};
    const auto generations = Generations(3, 1);
    const Kept kept{{Base, 1}, {Base + 1, 1}};
    const auto stamps = Walk(kept, layout, generations, 0, 3);
    for (const auto value : stamps) Require(value == Stamp, "zero-sized units hold no entry");
    Compare(kept, layout, generations, 0, 3, "zero-sized units");
}

void CheckLayoutsWithGaps() {
    const UnitLayout layout{Base, Block, 3 * Block / 2};
    const auto generations = Generations(4, 1);
    for (std::uint64_t key = Base; key < Base + 6 * Block; key += Block / 4) {
        const Kept kept{{key, 1}};
        for (std::uint32_t first = 0; first < 4; ++first) {
            for (std::uint32_t count = 0; first + count <= 4; ++count) Compare(kept, layout, generations, first, count, "units past the end of the image");
        }
    }
}

class Random {
public:
    explicit Random(std::uint64_t seed) : state(seed) {}

    std::uint64_t Next(std::uint64_t bound) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return bound == 0 ? 0 : (state >> 33) % bound;
    }

private:
    std::uint64_t state;
};

void CheckRandomShapes() {
    Random random(12345);
    const std::uint64_t unitSizes[] = {0, 1, 3, 4096, 65536, 65536, 65536, 100000};
    for (int round = 0; round < 40000; ++round) {
        UnitLayout layout;
        layout.base = random.Next(4) == 0 ? random.Next(1000) : Base + random.Next(0x10000) * 4096;
        layout.unitBytes = unitSizes[random.Next(8)];
        const auto units = static_cast<std::uint32_t>(1 + random.Next(12));
        layout.totalBytes = random.Next(8) == 0 ? random.Next(units * (layout.unitBytes + 1) + 1) : layout.unitBytes * units - (random.Next(2) == 0 ? 0 : random.Next(layout.unitBytes + 1));
        std::vector<std::uint64_t> generations(units);
        for (auto& generation : generations) generation = 1 + random.Next(3);
        Kept kept;
        const auto entries = random.Next(14);
        const auto span = layout.totalBytes + 2 * layout.unitBytes + 8;
        for (std::uint64_t entry = 0; entry < entries; ++entry) {
            const auto offset = random.Next(span + 16);
            const auto key = layout.base + offset - std::min<std::uint64_t>(offset, 8);
            kept[key] = 1 + random.Next(3);
        }
        const auto first = static_cast<std::uint32_t>(random.Next(units));
        const auto count = static_cast<std::uint32_t>(random.Next(units - first + 1));
        Compare(kept, layout, generations, first, count, "random shape " + std::to_string(round));
    }
}

}

int main() {
    try {
        CheckEmptyMap();
        CheckZeroCount();
        CheckKeptInsideOneUnit();
        CheckBoundaries();
        CheckRangeStartsInsideAndBeforeKeptRanges();
        CheckAdjacentAndOverlappingEntries();
        CheckLastUnitIsShort();
        CheckSingleUnitImage();
        CheckZeroSizedUnits();
        CheckLayoutsWithGaps();
        CheckRandomShapes();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "kept unit walk tests passed\n";
    return 0;
}
