#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WrittenBufferIndex.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using AgcDriver::DriverDetail::WrittenBuffer;
using AgcDriver::DriverDetail::WrittenBufferIndex;

constexpr std::uint64_t Base = std::uint64_t{1} << 32;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

WrittenBuffer Make(std::uint64_t begin, std::uint64_t end, std::uint64_t serial) {
    return WrittenBuffer{1, begin, end, serial, 0, false};
}

std::optional<WrittenBuffer> Scan(const WrittenBufferIndex& index, std::uint64_t begin, std::uint64_t end) {
    for (auto it = index.Entries().rbegin(); it != index.Entries().rend(); ++it) {
        if (begin < it->end && it->begin < end) return *it;
    }
    return std::nullopt;
}

void RequireSame(const WrittenBufferIndex& index, std::uint64_t begin, std::uint64_t end) {
    const auto expected = Scan(index, begin, end);
    const auto found = index.Newest(begin, end);
    if (expected.has_value() == found.has_value() && (!found || found->serial == expected->serial)) return;
    std::ostringstream text;
    text << std::hex << "query 0x" << begin << "..0x" << end << " differs from the linear scan";
    throw std::runtime_error(text.str());
}

std::uint64_t Length(std::mt19937_64& random) {
    const auto kind = random() % 100;
    if (kind < 3) return 0;
    if (kind < 70) return 4 * (1 + random() % 1024);
    if (kind < 95) return 4 * (1 + random() % 65536);
    return 0x200000 + random() % 0x600000;
}

void NewestOverlapWins() {
    WrittenBufferIndex index(8);
    index.Push(Make(Base, Base + 16, 1));
    index.Push(Make(Base + 8, Base + 32, 2));
    Require(index.Newest(Base + 8, Base + 12)->serial == 2, "the newer of two overlapping writers was not returned");
    Require(index.Newest(Base, Base + 4)->serial == 1, "the only writer of a range was not returned");
    Require(!index.Newest(Base + 32, Base + 36), "a range nothing wrote returned a writer");
}

void OldestEvicted() {
    WrittenBufferIndex index(2);
    index.Push(Make(Base, Base + 4, 1));
    index.Push(Make(Base + 0x100000, Base + 0x100004, 2));
    index.Push(Make(Base + 0x200000, Base + 0x200004, 3));
    Require(index.Entries().size() == 2, "the ring kept more entries than its capacity");
    Require(!index.Newest(Base, Base + 4), "an evicted writer was returned");
    Require(index.Newest(Base + 0x200000, Base + 0x200004)->serial == 3, "the newest writer was not returned after an eviction");
}

void MatchesLinearScan() {
    std::mt19937_64 random(0x5eed);
    WrittenBufferIndex index(64);
    for (std::uint64_t serial = 1; serial <= 20000; ++serial) {
        const auto begin = Base + 4 * (random() % (0x800000 / 4));
        index.Push(Make(begin, begin + Length(random), serial));
        Require(index.Entries().size() <= 64, "the ring kept more entries than its capacity");
        for (int query = 0; query < 8; ++query) {
            const auto at = Base + 4 * (random() % (0xa00000 / 4));
            RequireSame(index, at, at + Length(random));
        }
    }
}

}

int main() {
    try {
        NewestOverlapWins();
        OldestEvicted();
        MatchesLinearScan();
    } catch (const std::exception& error) {
        std::cerr << "written buffer index tests failed: " << error.what() << "\n";
        return 1;
    }
    std::cout << "written buffer index tests passed\n";
    return 0;
}
