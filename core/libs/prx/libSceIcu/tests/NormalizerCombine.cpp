#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <stdexcept>

namespace {

struct Rng {
    std::uint64_t state;

    std::uint32_t Next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 32);
    }

    std::uint32_t Below(std::uint32_t bound) {
        return Next() % bound;
    }
};

constexpr std::size_t Padding = 6;
constexpr std::int32_t MaxCodePoint = 0x10ffff;

std::uint16_t Key1(std::int32_t trail) {
    const auto bits = static_cast<std::uint32_t>(trail);
    if (trail < 0x3400) {
        return static_cast<std::uint16_t>(bits << 1);
    }
    return static_cast<std::uint16_t>(0x3400 + ((trail >> 9) & ~1));
}

void AddEntry(std::vector<std::uint16_t>& list, Rng& rng, std::int32_t trail) {
    const auto composite = rng.Next();
    const auto bits = static_cast<std::uint32_t>(trail);
    if (trail < 0x3400) {
        if (rng.Below(2) != 0) {
            list.push_back(static_cast<std::uint16_t>((bits << 1) | 1));
            list.push_back(static_cast<std::uint16_t>(composite >> 16));
        } else {
            list.push_back(static_cast<std::uint16_t>(bits << 1));
        }
        list.push_back(static_cast<std::uint16_t>(composite));
        return;
    }
    list.push_back(static_cast<std::uint16_t>(Key1(trail) | 1));
    list.push_back(static_cast<std::uint16_t>(((bits << 6) & 0xffc0) | (composite >> 26)));
    list.push_back(static_cast<std::uint16_t>(composite));
}

std::vector<std::int32_t> PickTrails(Rng& rng, std::uint32_t kind) {
    std::vector<std::int32_t> trails;
    const auto count = 1 + rng.Below(40);
    while (trails.size() < count) {
        const auto pick = rng.Below(4);
        std::int32_t trail;
        if (pick == 0) {
            trail = static_cast<std::int32_t>(rng.Below(kind == 0 ? 0x1a00 : 0x3400));
        } else if (pick == 1 && !trails.empty()) {
            trail = trails.back() + 1 + static_cast<std::int32_t>(rng.Below(600));
        } else {
            trail = 0x3400 + static_cast<std::int32_t>(rng.Below(MaxCodePoint + 1 - 0x3400));
        }
        if (trail <= MaxCodePoint) {
            trails.push_back(trail);
        }
    }
    std::sort(trails.begin(), trails.end());
    trails.erase(std::unique(trails.begin(), trails.end()), trails.end());
    return trails;
}

struct Corpus {
    std::uint32_t cases;
    std::uint32_t found;
    std::uint32_t hash;
};

template <typename Combine>
Corpus RunCorpus(std::uint64_t seed, std::uint32_t lists, Combine combine) {
    Rng rng{seed};
    Corpus corpus{0, 0, 2166136261u};
    std::vector<std::uint16_t> list;
    std::vector<std::int32_t> queries;
    for (std::uint32_t n = 0; n < lists; ++n) {
        list.clear();
        queries.clear();
        const auto kind = rng.Below(3);
        if (kind == 2) {
            const auto count = 1 + rng.Below(80);
            for (std::uint32_t i = 0; i < count; ++i) {
                list.push_back(static_cast<std::uint16_t>(rng.Next()));
            }
            for (int i = 0; i < 8; ++i) {
                queries.push_back(list[rng.Below(static_cast<std::uint32_t>(list.size()))] >> 1);
            }
        } else {
            const auto trails = PickTrails(rng, kind);
            std::size_t last = 0;
            for (const auto trail : trails) {
                last = list.size();
                AddEntry(list, rng, trail);
                queries.push_back(trail - 1);
                queries.push_back(trail);
                queries.push_back(trail + 1);
            }
            list[last] = static_cast<std::uint16_t>(list[last] | 0x8000);
        }
        list.insert(list.end(), Padding, 0xffff);
        queries.push_back(0);
        queries.push_back(0x33ff);
        queries.push_back(0x3400);
        queries.push_back(MaxCodePoint);
        for (int i = 0; i < 4; ++i) {
            queries.push_back(static_cast<std::int32_t>(rng.Below(MaxCodePoint + 1)));
            queries.push_back(static_cast<std::int32_t>(rng.Next()));
        }
        for (const auto trail : queries) {
            const auto result = static_cast<std::uint32_t>(combine(list.data(), trail));
            ++corpus.cases;
            if (result != 0xffffffffu) {
                ++corpus.found;
            }
            for (int shift = 0; shift < 32; shift += 8) {
                corpus.hash = (corpus.hash ^ ((result >> shift) & 0xff)) * 16777619u;
            }
        }
    }
    return corpus;
}

const std::uint16_t SmallList[] = {0x0082, 0x00c4, 0x0603, 0x0001, 0xe000, 0x8682, 0x0f71, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::uint16_t LargeList[] = {0x0300, 0x00e0, 0x3489, 0x49c1, 0x112e, 0x3489, 0x4a82, 0x112f, 0xbc7f, 0xffff, 0x2345,
                                   0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::uint16_t LastList[] = {0x0082, 0x00c4, 0x8682, 0x0f71, 0x0603, 0x0001, 0xe000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::uint16_t LastLargeList[] = {0xb489, 0x49c1, 0x112e, 0x3489, 0x4a82, 0x112f, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::uint16_t LastLargeLowList[] = {0xb489, 0x4a82, 0x112f, 0x3489, 0x49c1, 0x112e, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::uint16_t TwoUnitList[] = {0xb66a, 0x360c, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
const std::int32_t Trails[] = {0x0000, 0x0040, 0x0041, 0x0042, 0x0180, 0x0300, 0x0301, 0x0302, 0x0341, 0x0342, 0x33ff,
                               0x3400, 0x11126, 0x11127, 0x11128, 0x1112a, 0x1112b, 0x10fffe, 0x10ffff, -32768, -65536,
                               0x7fffffff, 0x00800000, 0x00600123, -65471, 0x02011127, 0x4d4d8, 0x211127};

const std::int32_t SmallExpected[] = {-1, -1, 196, -1, -1, -1, 122880, -1, 3953, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 196, -1, -1, -1};
const std::int32_t LargeExpected[] = {-1, -1, -1, -1, 224, -1, -1, -1, -1, -1, -1, -1, -1, 69934, -1, 135471, -1, -1, 4137797, -1, -1, -1, -1, -1, -1, 69934, -1, -1};
const std::int32_t LastExpected[] = {-1, -1, 196, -1, -1, -1, -1, -1, 3953, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 196, -1, -1, -1};
const std::int32_t LastLargeExpected[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 69934, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 69934, -1, -1};
const std::int32_t LastLargeLowExpected[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 135471, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
const std::int32_t TwoUnitExpected[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 851967, -1};

int failures = 0;

std::int32_t ProjectCombine(const std::uint16_t* list, std::int32_t trail) {
    return _ZN6icu_6815Normalizer2Impl7combineEPKti_nid_postfix(list, trail);
}

void ExpectEqual(long long actual, long long expected, const char* what, long long detail) {
    if (actual != expected) {
        std::fprintf(stderr, "%s %lld: got %lld, expected %lld\n", what, detail, actual, expected);
        ++failures;
    }
}

void TestVectors() {
    constexpr std::size_t count = sizeof(Trails) / sizeof(Trails[0]);
    static_assert(sizeof(SmallExpected) / sizeof(SmallExpected[0]) == count);
    static_assert(sizeof(LargeExpected) / sizeof(LargeExpected[0]) == count);
    static_assert(sizeof(LastExpected) / sizeof(LastExpected[0]) == count);
    static_assert(sizeof(LastLargeExpected) / sizeof(LastLargeExpected[0]) == count);
    static_assert(sizeof(LastLargeLowExpected) / sizeof(LastLargeLowExpected[0]) == count);
    static_assert(sizeof(TwoUnitExpected) / sizeof(TwoUnitExpected[0]) == count);
    for (std::size_t i = 0; i < count; ++i) {
        ExpectEqual(ProjectCombine(SmallList, Trails[i]), SmallExpected[i], "small list, trail", Trails[i]);
        ExpectEqual(ProjectCombine(LargeList, Trails[i]), LargeExpected[i], "large list, trail", Trails[i]);
        ExpectEqual(ProjectCombine(LastList, Trails[i]), LastExpected[i], "list followed by data, trail", Trails[i]);
        ExpectEqual(ProjectCombine(LastLargeList, Trails[i]), LastLargeExpected[i], "large list followed by data, trail", Trails[i]);
        ExpectEqual(ProjectCombine(LastLargeLowList, Trails[i]), LastLargeLowExpected[i], "large list followed by lower data, trail", Trails[i]);
        ExpectEqual(ProjectCombine(TwoUnitList, Trails[i]), TwoUnitExpected[i], "two-unit entry with a large key, trail", Trails[i]);
    }
}

void TestNullList() {
    const std::int32_t trails[] = {0x0000, 0x33ff, 0x3400, 0x10ffff};
    for (const auto trail : trails) {
        bool threw = false;
        try {
            (void)_ZN6icu_6815Normalizer2Impl7combineEPKti_nid_postfix(nullptr, trail);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) {
            std::fprintf(stderr, "null list, trail %#x: expected std::invalid_argument\n", trail);
            ++failures;
        }
    }
}

void TestCorpus() {
    const struct {
        std::uint64_t seed;
        Corpus expected;
    } runs[] = {
        {0x5eed0001u, {221267, 38271, 0x8d6953abu}},
        {0x5eed0002u, {222170, 36701, 0x21eba74bu}},
        {0x5eed0003u, {223869, 38611, 0x619a6146u}},
    };
    for (const auto& run : runs) {
        const auto corpus = RunCorpus(run.seed, 4000, ProjectCombine);
        ExpectEqual(corpus.cases, run.expected.cases, "corpus cases, seed", static_cast<long long>(run.seed));
        ExpectEqual(corpus.found, run.expected.found, "corpus compositions, seed", static_cast<long long>(run.seed));
        ExpectEqual(corpus.hash, run.expected.hash, "corpus hash, seed", static_cast<long long>(run.seed));
    }
}

} // namespace

int main() {
    TestVectors();
    TestNullList();
    TestCorpus();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
