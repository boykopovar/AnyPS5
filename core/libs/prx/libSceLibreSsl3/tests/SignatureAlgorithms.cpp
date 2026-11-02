#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

constexpr std::array<std::array<int, 3>, 51> Expected = {{
    {7, 3, 6},
    {8, 4, 6},
    {42, 41, 6},
    {65, 64, 6},
    {66, 41, 116},
    {70, 64, 67},
    {96, 95, 6},
    {104, 4, 19},
    {113, 64, 116},
    {115, 64, 19},
    {119, 117, 6},
    {396, 257, 6},
    {416, 64, 408},
    {668, 672, 6},
    {669, 673, 6},
    {670, 674, 6},
    {671, 675, 6},
    {791, 0, 408},
    {792, 0, 408},
    {793, 675, 408},
    {794, 672, 408},
    {795, 673, 408},
    {796, 674, 408},
    {802, 675, 116},
    {803, 672, 116},
    {807, 809, 811},
    {808, 809, 812},
    {852, 809, 850},
    {853, 809, 851},
    {912, 0, 912},
    {948, 941, 811},
    {949, 942, 811},
    {952, 0, 952},
    {980, 64, 990},
    {981, 675, 990},
    {982, 672, 990},
    {983, 673, 990},
    {984, 674, 990},
    {985, 64, 991},
    {986, 675, 991},
    {987, 672, 991},
    {988, 673, 991},
    {989, 674, 991},
    {1049, 1031, 6},
    {1050, 1032, 6},
    {1051, 1033, 6},
    {1052, 1034, 6},
    {1045, 1031, 408},
    {1046, 1032, 408},
    {1047, 1033, 408},
    {1048, 1034, 408},
}};

std::size_t checks = 0;
std::size_t cases = 0;

void Require(bool condition) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "signature mapping check %zu failed in case %zu\n", checks, cases);
        std::abort();
    }
}

struct Guarded {
    std::uint32_t before = 0x91ab72c3;
    int value = -17019;
    std::uint32_t after = 0x48d05e2f;
};

void CheckGuard(const Guarded& output) {
    Require(output.before == 0x91ab72c3);
    Require(output.after == 0x48d05e2f);
}

void RunCase(int signatureNid) {
    ++cases;
    const std::array<int, 3>* expected = nullptr;
    for (const auto& row : Expected)
        if (row[0] == signatureNid)
            expected = &row;
    const int status = expected ? 1 : 0;
    for (unsigned mask = 0; mask != 4; ++mask) {
        Guarded digest;
        Guarded key;
        Require(OBJ_find_sigid_algs_nid_postfix(signatureNid,
            mask & 1 ? &digest.value : nullptr, mask & 2 ? &key.value : nullptr) == status);
        Require(digest.value == (expected && (mask & 1) ? (*expected)[1] : -17019));
        Require(key.value == (expected && (mask & 2) ? (*expected)[2] : -17019));
        CheckGuard(digest);
        CheckGuard(key);
    }
    Guarded aliased;
    Require(OBJ_find_sigid_algs_nid_postfix(signatureNid, &aliased.value, &aliased.value) == status);
    Require(aliased.value == (expected ? (*expected)[2] : -17019));
    CheckGuard(aliased);
    Guarded first;
    Guarded second;
    Require(OBJ_find_sigid_algs_nid_postfix(signatureNid, &second.value, &first.value) == status);
    Require(first.value == (expected ? (*expected)[2] : -17019));
    Require(second.value == (expected ? (*expected)[1] : -17019));
    CheckGuard(first);
    CheckGuard(second);
}

}

int main() {
    static_assert(sizeof(int) == sizeof(std::uint32_t));
    for (const auto& row : Expected)
        RunCase(row[0]);
    for (int nid = -4096; nid <= 4096; ++nid)
        RunCase(nid);
    RunCase(std::numeric_limits<int>::min());
    RunCase(std::numeric_limits<int>::max());
    for (unsigned bit = 0; bit != 32; ++bit) {
        const std::uint32_t value = std::uint32_t{1} << bit;
        RunCase(std::bit_cast<int>(value));
        RunCase(std::bit_cast<int>(~value));
    }
    std::uint32_t state = 0x8e1b0537;
    for (unsigned i = 0; i != 4096; ++i) {
        state = state * 1664525u + 1013904223u;
        RunCase(std::bit_cast<int>(state));
    }
    std::printf("signature mappings: %zu cases, %zu checks, %zu mappings\n", cases, checks, Expected.size());
}
