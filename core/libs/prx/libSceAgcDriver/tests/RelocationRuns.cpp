#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/RelocationRuns.hpp"
#include <cstdio>
#include <vector>

namespace {

struct Run {
    std::size_t first;
    std::size_t last;
    std::int8_t origin;
    bool operator==(const Run&) const = default;
};

using Reads = std::vector<std::pair<std::uint64_t, std::int32_t>>;

std::vector<Run> Reference(std::uint64_t address, std::size_t words, const Reads& reads, std::uint64_t mask) {
    const auto origin = [&](std::uint64_t at) -> std::int8_t {
        const auto read = std::lower_bound(reads.begin(), reads.end(), std::make_pair(at, std::numeric_limits<std::int32_t>::min()));
        if (read == reads.end() || read->first != at || read->second < 0) return -1;
        return (mask >> read->second & 1u) != 0 ? static_cast<std::int8_t>(read->second) : std::int8_t{-1};
    };
    std::vector<Run> result;
    std::size_t start = 0;
    auto current = words != 0 ? origin(address) : std::int8_t{-1};
    for (std::size_t word = 1; word <= words; ++word) {
        const auto next = word < words ? origin(address + word * 4u) : std::int8_t{-2};
        if (next == current) continue;
        result.push_back({start, word, current});
        start = word;
        current = next;
    }
    return result;
}

bool Check(std::uint64_t address, std::size_t words, Reads reads, std::uint64_t mask, unsigned seed) {
    std::sort(reads.begin(), reads.end());
    std::vector<Run> result;
    AgcDriver::DriverDetail::ForEachRelocationRun(address, words, reads, mask, [&](std::size_t first, std::size_t last, std::int8_t origin) { result.push_back({first, last, origin}); });
    if (result == Reference(address, words, reads, mask)) return true;
    std::fprintf(stderr, "relocation run mismatch: seed %u, %zu words, %zu reads, %zu / %zu runs\n", seed, words, reads.size(), result.size(), Reference(address, words, reads, mask).size());
    return false;
}

}

int main() {
    bool pass = true;
    const Reads boundaries{{0xffc, 1}, {0x1000, 0}, {0x1004, 0}, {0x1008, 63}, {0x100c, -1}, {0x1010, 2}, {0x1010, 3}, {0x1014, -1}, {0x1014, 5}, {0x1019, 6}, {0x103c, 0}, {0x1040, 1}};
    for (const auto words : {std::size_t{0}, std::size_t{1}, std::size_t{16}, std::size_t{17}}) {
        for (const auto mask : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{1} << 63u, ~std::uint64_t{0}}) pass = Check(0x1000, words, boundaries, mask, 0) && pass;
    }
    pass = Check(0x1001, 16, boundaries, ~std::uint64_t{0}, 1) && pass;
    pass = Check(0x1000, 32, {}, ~std::uint64_t{0}, 2) && pass;
    pass = Check(0x1000, 1u << 20u, {{0x1000, 1}, {0x101000, 2}, {0x400ffc, 63}}, ~std::uint64_t{0}, 3) && pass;
    pass = Check(0xffffffffffffffbfull, 16, {{0xffffffffffffffbfull, 1}, {0xfffffffffffffffbull, 63}}, ~std::uint64_t{0}, 4) && pass;
    std::uint64_t state = 0x4d595df4d0f33173ull;
    const auto random = [&]() { state ^= state << 13u; state ^= state >> 7u; state ^= state << 17u; return state; };
    for (unsigned seed = 5; seed < 2005; ++seed) {
        const auto address = std::uint64_t{0x1000} + random() % 4u;
        const auto words = static_cast<std::size_t>(random() % 258u);
        const auto count = random() % 128u;
        Reads reads;
        for (std::size_t i = 0; i < count; ++i) reads.emplace_back(0xff0u + random() % 280u * 4u + random() % 4u, static_cast<std::int32_t>(random() % 65u) - 1);
        pass = Check(address, words, std::move(reads), random(), seed) && pass;
    }
    if (!pass) return 1;
    std::puts("relocation run partitions match the per-word reference");
    return 0;
}
