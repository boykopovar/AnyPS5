#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t MaxCodePoint = 0x10ffff;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectAt(bool condition, const char* what, std::int64_t first, std::int64_t second) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s at 0x%llx 0x%llx\n", what, static_cast<unsigned long long>(first),
                 static_cast<unsigned long long>(second));
    std::abort();
}

template <typename Function>
void ExpectAnyThrow(Function function, const char* what) {
    try {
        function();
    } catch (...) {
        return;
    }
    Expect(false, what);
}

template <typename Function>
void ExpectInvalid(Function function, const char* what) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Expect(false, what);
}

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

std::int32_t Pick(Rng& rng) {
    static const std::int32_t pool[] = {'A', 'E', 'a', 'e', 'Z', 0x301, 0x300, 0x308, 0x327, 0x1100, 0x1161, 0x11a8, 0xac00, 0x0915, 0x093c};
    switch (rng.Below(3)) {
    case 0:
        return pool[rng.Below(sizeof(pool) / sizeof(pool[0]))];
    case 1:
        return static_cast<std::int32_t>(rng.Below(0x400));
    default:
        return static_cast<std::int32_t>(rng.Next());
    }
}

void Compare(const Icu::Normalizer2* base, const USet* filter, std::int32_t minimumComposites, const char* what) {
    std::int32_t error = Success;
    auto* filtered = unorm2_openFiltered_nid_postfix(base, filter, &error);
    Expect(filtered != nullptr && error == Success, "unorm2_openFiltered");

    std::int32_t inertDifferences = 0;
    for (std::int32_t c = -1; c <= MaxCodePoint + 1; ++c) {
        const bool inFilter = uset_contains_nid_postfix(filter, c) != 0;
        const bool inert = _ZNK6icu_6819FilteredNormalizer27isInertEi_nid_postfix(filtered, c) != 0;
        const bool boundary = _ZNK6icu_6819FilteredNormalizer217hasBoundaryBeforeEi_nid_postfix(filtered, c) != 0;
        ExpectAt(inert == (unorm2_isInert_nid_postfix(filtered, c) != 0), "isInert matches unorm2_isInert", c, 0);
        ExpectAt(boundary == (unorm2_hasBoundaryBefore_nid_postfix(filtered, c) != 0), "hasBoundaryBefore matches unorm2_hasBoundaryBefore",
                 c, 0);
        if (c >= 0 && c <= MaxCodePoint) {
            ExpectAt(inert == (!inFilter || unorm2_isInert_nid_postfix(base, c) != 0), "isInert outside the filter", c, 0);
            ExpectAt(boundary == (!inFilter || unorm2_hasBoundaryBefore_nid_postfix(base, c) != 0), "hasBoundaryBefore outside the filter",
                     c, 0);
            inertDifferences += inert != (unorm2_isInert_nid_postfix(base, c) != 0);
        }
    }
    Expect(inertDifferences > 0, what);

    Rng rng{0x2545f4914f6cdd1dull};
    std::int32_t composites = 0;
    for (std::int32_t i = 0; i < 300000; ++i) {
        const auto first = Pick(rng);
        const auto second = Pick(rng);
        const auto actual = _ZNK6icu_6819FilteredNormalizer211composePairEii_nid_postfix(filtered, first, second);
        ExpectAt(actual == unorm2_composePair_nid_postfix(filtered, first, second), "composePair matches unorm2_composePair", first, second);
        const bool bothIn = uset_contains_nid_postfix(filter, first) != 0 && uset_contains_nid_postfix(filter, second) != 0;
        ExpectAt(actual == (bothIn ? unorm2_composePair_nid_postfix(base, first, second) : -1), "composePair outside the filter", first,
                 second);
        composites += actual >= 0;
    }
    Expect(composites >= minimumComposites, "composePair found composites");
    unorm2_close_nid_postfix(filtered);
}

}

int main() {
    std::int32_t error = Success;
    const auto* nfc = unorm2_getNFCInstance_nid_postfix(&error);
    const auto* nfd = unorm2_getNFDInstance_nid_postfix(&error);
    const auto* nfkc = unorm2_getNFKCInstance(&error);
    Expect(nfc && nfd && nfkc && error == Success, "base normalizers");

    auto* filter = uset_openEmpty_nid_postfix();
    uset_addRange_nid_postfix(filter, 'A', 'Z');
    uset_addRange_nid_postfix(filter, 0x300, 0x30f);
    uset_addRange_nid_postfix(filter, 0x1100, 0x11ff);
    uset_addRange_nid_postfix(filter, 0xac00, 0xac10);
    uset_addRange_nid_postfix(filter, 0x0900, 0x097f);
    uset_add_nid_postfix(filter, 0xfb01);
    uset_add_nid_postfix(filter, 0x1d15e);
    Compare(nfc, filter, 100, "NFC filter changes inertness");
    Compare(nfd, filter, 100, "NFD filter changes inertness");
    Compare(nfkc, filter, 100, "NFKC filter changes inertness");
    const auto* whiteSpace = u_getBinaryPropertySet_nid_postfix(31, &error);
    Expect(whiteSpace && error == Success, "White_Space set");
    Compare(nfkc, whiteSpace, 0, "NFKC White_Space filter changes inertness");

    ExpectAnyThrow([&] { _ZNK6icu_6819FilteredNormalizer211composePairEii_nid_postfix(nfc, 'A', 0x301); },
                   "composePair on a normalizer that is not filtered");
    ExpectAnyThrow([&] { _ZNK6icu_6819FilteredNormalizer27isInertEi_nid_postfix(nfd, 'A'); }, "isInert on a normalizer that is not filtered");
    ExpectAnyThrow([&] { _ZNK6icu_6819FilteredNormalizer217hasBoundaryBeforeEi_nid_postfix(nfkc, 'A'); },
                   "hasBoundaryBefore on a normalizer that is not filtered");
    ExpectInvalid([] { _ZNK6icu_6819FilteredNormalizer211composePairEii_nid_postfix(nullptr, 'A', 0x301); }, "composePair(null)");
    ExpectInvalid([] { _ZNK6icu_6819FilteredNormalizer27isInertEi_nid_postfix(nullptr, 'A'); }, "isInert(null)");
    ExpectInvalid([] { _ZNK6icu_6819FilteredNormalizer217hasBoundaryBeforeEi_nid_postfix(nullptr, 'A'); }, "hasBoundaryBefore(null)");
    uset_close_nid_postfix(filter);
    std::puts("filtered normalizer query tests passed");
    return 0;
}
