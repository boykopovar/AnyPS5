#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t ModeNone = 1;
constexpr std::int32_t ModeNfd = 2;
constexpr std::int32_t ModeNfkd = 3;
constexpr std::int32_t ModeNfc = 4;
constexpr std::int32_t ModeNfkc = 5;
constexpr std::int32_t ModeFcd = 6;
constexpr std::int32_t MaxCodePoint = 0x10ffff;
constexpr std::int32_t Capacity = 64;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

void ExpectAt(bool condition, const char* what, std::int64_t value) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s at 0x%llx\n", what, static_cast<unsigned long long>(value));
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

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
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

template <typename TGetter>
const Icu::Normalizer2* Instance(TGetter getter, const char* what) {
    std::int32_t error = Success;
    const auto* normalizer = getter(&error);
    ExpectInt(error, Success, what);
    Expect(normalizer != nullptr, what);
    return normalizer;
}

const Icu::Normalizer2* FactoryInstance(std::int32_t mode, const char* what) {
    std::int32_t error = Success;
    const auto* normalizer = _ZN6icu_6818Normalizer2Factory11getInstanceE18UNormalizationModeR10UErrorCode_nid_postfix(mode, &error);
    ExpectInt(error, Success, what);
    Expect(normalizer != nullptr, what);
    return normalizer;
}

const Icu::Normalizer2* Noop() {
    std::int32_t error = Success;
    const auto* normalizer = _ZN6icu_6818Normalizer2Factory15getNoopInstanceER10UErrorCode_nid_postfix(&error);
    ExpectInt(error, Success, "getNoopInstance status");
    Expect(normalizer != nullptr, "getNoopInstance");
    return normalizer;
}

const Icu::Normalizer2Impl* NfkcImpl() {
    std::int32_t error = Success;
    const auto* impl = _ZN6icu_6818Normalizer2Factory11getNFKCImplER10UErrorCode_nid_postfix(&error);
    ExpectInt(error, Success, "getNFKCImpl status");
    Expect(impl != nullptr, "getNFKCImpl");
    return impl;
}

const Icu::Normalizer2Impl* NfkcCasefoldImpl() {
    std::int32_t error = Success;
    const auto* impl = _ZN6icu_6818Normalizer2Factory14getNFKC_CFImplER10UErrorCode_nid_postfix(&error);
    ExpectInt(error, Success, "getNFKC_CFImpl status");
    Expect(impl != nullptr, "getNFKC_CFImpl");
    return impl;
}

const Icu::Normalizer2* Nfc() {
    return Instance(_ZN6icu_6811Normalizer214getNFCInstanceER10UErrorCode_nid_postfix, "Normalizer2::getNFCInstance");
}

const Icu::Normalizer2* Nfd() {
    return Instance(_ZN6icu_6811Normalizer214getNFDInstanceER10UErrorCode_nid_postfix, "Normalizer2::getNFDInstance");
}

const Icu::Normalizer2* NfkcCasefold() {
    return Instance(_ZN6icu_6811Normalizer223getNFKCCasefoldInstanceER10UErrorCode_nid_postfix,
                    "Normalizer2::getNFKCCasefoldInstance");
}

const Icu::Normalizer2* Nfkc() {
    return Instance(unorm2_getNFKCInstance, "unorm2_getNFKCInstance");
}

const Icu::Normalizer2* Nfkd() {
    return Instance(unorm2_getNFKDInstance, "unorm2_getNFKDInstance");
}

void TestInstances() {
    Expect(Nfc() == Instance(unorm2_getNFCInstance_nid_postfix, "unorm2_getNFCInstance"), "getNFCInstance matches unorm2");
    Expect(Nfd() == Instance(unorm2_getNFDInstance_nid_postfix, "unorm2_getNFDInstance"), "getNFDInstance matches unorm2");
    Expect(NfkcCasefold() == Instance(unorm2_getNFKCCasefoldInstance_nid_postfix, "unorm2_getNFKCCasefoldInstance"),
           "getNFKCCasefoldInstance matches unorm2");
    Expect(Nfc() != Nfd() && Nfc() != NfkcCasefold(), "distinct instances");

    Expect(FactoryInstance(ModeNfd, "getInstance(NFD)") == Nfd(), "getInstance(NFD)");
    Expect(FactoryInstance(ModeNfc, "getInstance(NFC)") == Nfc(), "getInstance(NFC)");
    Expect(FactoryInstance(ModeNfkd, "getInstance(NFKD)") == Nfkd(), "getInstance(NFKD)");
    Expect(FactoryInstance(ModeNfkc, "getInstance(NFKC)") == Nfkc(), "getInstance(NFKC)");
    Expect(FactoryInstance(ModeNone, "getInstance(NONE)") == Noop(), "getInstance(NONE)");
    for (const std::int32_t mode : {0, 7, 99, -1}) {
        Expect(FactoryInstance(mode, "getInstance(other)") == Noop(), "getInstance(other mode) is the no-op instance");
    }
    const auto* fcd = FactoryInstance(ModeFcd, "getInstance(FCD)");
    Expect(fcd != Noop() && fcd != Nfc() && fcd != Nfd(), "getInstance(FCD)");
    Expect(Noop() == Noop(), "getNoopInstance is a singleton");

    for (const auto getter : {_ZN6icu_6811Normalizer214getNFCInstanceER10UErrorCode_nid_postfix,
                              _ZN6icu_6811Normalizer214getNFDInstanceER10UErrorCode_nid_postfix,
                              _ZN6icu_6811Normalizer223getNFKCCasefoldInstanceER10UErrorCode_nid_postfix,
                              _ZN6icu_6818Normalizer2Factory15getNoopInstanceER10UErrorCode_nid_postfix}) {
        std::int32_t failing = IllegalArgument;
        Expect(getter(&failing) == nullptr, "instance with failing status");
        ExpectInt(failing, IllegalArgument, "instance with failing status keeps it");
        ExpectThrow<std::invalid_argument>([&] { getter(nullptr); }, "instance with null status");
    }
    std::int32_t failing = IllegalArgument;
    Expect(_ZN6icu_6818Normalizer2Factory11getInstanceE18UNormalizationModeR10UErrorCode_nid_postfix(ModeNfc, &failing) == nullptr,
           "getInstance with failing status");
    ExpectInt(failing, IllegalArgument, "getInstance with failing status keeps it");
    ExpectThrow<std::invalid_argument>(
        [] { _ZN6icu_6818Normalizer2Factory11getInstanceE18UNormalizationModeR10UErrorCode_nid_postfix(ModeNfc, nullptr); },
        "getInstance with null status");
}

void TestImpls() {
    const auto* nfkc = NfkcImpl();
    const auto* casefold = NfkcCasefoldImpl();
    Expect(nfkc != casefold, "distinct impls");
    Expect(_ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Nfkc()) == nfkc, "getImpl(NFKC)");
    Expect(_ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Nfkd()) == nfkc, "getImpl(NFKD)");
    Expect(_ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(NfkcCasefold()) == casefold, "getImpl(NFKC_CF)");
    const auto* nfcImpl = _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Nfc());
    Expect(nfcImpl != nullptr && nfcImpl != nfkc && nfcImpl != casefold, "getImpl(NFC)");
    Expect(_ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Nfd()) == nfcImpl, "getImpl(NFD)");
    Expect(_ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(FactoryInstance(ModeFcd, "FCD")) == nfcImpl,
           "getImpl(FCD)");
    ExpectAnyThrow([] { _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Noop()); }, "getImpl(no-op)");
    ExpectThrow<std::invalid_argument>([] { _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(nullptr); },
                                       "getImpl(null)");

    for (const auto getter : {_ZN6icu_6818Normalizer2Factory11getNFKCImplER10UErrorCode_nid_postfix,
                              _ZN6icu_6818Normalizer2Factory14getNFKC_CFImplER10UErrorCode_nid_postfix}) {
        std::int32_t failing = IllegalArgument;
        Expect(getter(&failing) == nullptr, "impl with failing status");
        ExpectInt(failing, IllegalArgument, "impl with failing status keeps it");
        ExpectThrow<std::invalid_argument>([&] { getter(nullptr); }, "impl with null status");
    }
}

void TestBaseMethods() {
    Rng rng{0x9e3779b97f4a7c15ull};
    const Icu::Normalizer2* normalizers[] = {Nfc(), Nfd(), Nfkc(), NfkcCasefold(), Noop()};
    for (const auto* normalizer : normalizers) {
        for (std::int32_t i = 0; i < 4096; ++i) {
            const auto first = static_cast<std::int32_t>(rng.Next());
            const auto second = static_cast<std::int32_t>(rng.Below(MaxCodePoint + 1));
            ExpectInt(_ZNK6icu_6811Normalizer211composePairEii_nid_postfix(normalizer, first, second), -1, "Normalizer2::composePair");
            ExpectInt(_ZNK6icu_6811Normalizer217getCombiningClassEi_nid_postfix(normalizer, second), 0,
                      "Normalizer2::getCombiningClass");
        }
        ExpectInt(_ZNK6icu_6811Normalizer211composePairEii_nid_postfix(normalizer, 'A', 0x301), -1, "Normalizer2::composePair(A, acute)");
        ExpectInt(_ZNK6icu_6811Normalizer217getCombiningClassEi_nid_postfix(normalizer, 0x301), 0,
                  "Normalizer2::getCombiningClass(acute)");
    }
    ExpectThrow<std::invalid_argument>([] { _ZNK6icu_6811Normalizer211composePairEii_nid_postfix(nullptr, 'A', 0x301); },
                                       "Normalizer2::composePair(null)");
    ExpectThrow<std::invalid_argument>([] { _ZNK6icu_6811Normalizer217getCombiningClassEi_nid_postfix(nullptr, 0x301); },
                                       "Normalizer2::getCombiningClass(null)");
}

void AppendUtf8(std::string& out, std::int32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xc0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xe0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (c & 0x3f));
    }
}

void AppendUtf16(std::u16string& out, std::int32_t c) {
    if (c < 0x10000) {
        out += static_cast<char16_t>(c);
        return;
    }
    out += static_cast<char16_t>(0xd7c0 + (c >> 10));
    out += static_cast<char16_t>(0xdc00 | (c & 0x3ff));
}

std::int32_t PickCodePoint(Rng& rng) {
    static const std::int32_t pool[] = {'A', 'a', 'e', 'o', 0x301, 0x300, 0x308, 0x323, 0x327, 0x345, 0x5b0, 0x1100, 0x1161, 0x11a8,
                                        0xac00, 0xc0, 0xe9, 0x1e0a, 0x212b, 0x2126, 0xfb01, 0x3392, 0xff21, 0x1d15e, 0x2f800, 0x130};
    switch (rng.Below(3)) {
    case 0:
        return pool[rng.Below(sizeof(pool) / sizeof(pool[0]))];
    case 1:
        return static_cast<std::int32_t>(rng.Below(0x3000));
    default: {
        std::int32_t c = static_cast<std::int32_t>(rng.Below(MaxCodePoint + 1));
        return c >= 0xd800 && c <= 0xdfff ? 0xfffd : c;
    }
    }
}

void TestIsNormalizedUtf8() {
    Rng rng{0x1234567887654321ull};
    const Icu::Normalizer2* normalizers[] = {Nfc(), Nfd(), Nfkc(), Nfkd(), NfkcCasefold(), Noop()};
    std::int32_t yes = 0;
    std::int32_t no = 0;
    for (std::int32_t round = 0; round < 6000; ++round) {
        std::string utf8;
        std::u16string utf16;
        const auto count = rng.Below(8);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto c = PickCodePoint(rng);
            AppendUtf8(utf8, c);
            AppendUtf16(utf16, c);
        }
        for (const auto* normalizer : normalizers) {
            std::int32_t expectedError = Success;
            const auto expected =
                unorm2_isNormalized_nid_postfix(normalizer, utf16.data(), static_cast<std::int32_t>(utf16.size()), &expectedError);
            ExpectInt(expectedError, Success, "unorm2_isNormalized status");
            std::int32_t error = Success;
            const auto actual = _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(
                normalizer, utf8.data(), static_cast<std::int32_t>(utf8.size()), &error);
            ExpectInt(error, Success, "isNormalizedUTF8 status");
            ExpectInt(actual != 0, expected != 0, "isNormalizedUTF8 matches unorm2_isNormalized");
            (actual != 0 ? yes : no) += 1;
        }
    }
    Expect(yes > 1000 && no > 1000, "isNormalizedUTF8 saw both answers");

    const std::string composed = "\xc3\x81";
    const std::string decomposed = "A\xcc\x81";
    auto check = [](const Icu::Normalizer2* normalizer, const std::string& text) {
        std::int32_t error = Success;
        const auto result = _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(
            normalizer, text.data(), static_cast<std::int32_t>(text.size()), &error);
        ExpectInt(error, Success, "isNormalizedUTF8 status");
        return result != 0;
    };
    Expect(check(Nfc(), composed) && !check(Nfc(), decomposed), "isNormalizedUTF8 NFC");
    Expect(!check(Nfd(), composed) && check(Nfd(), decomposed), "isNormalizedUTF8 NFD");
    Expect(check(Nfc(), std::string()), "isNormalizedUTF8 empty");

    const char terminated[] = "A\xcc\x81";
    std::int32_t error = Success;
    ExpectInt(_ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(Nfc(), terminated, -1, &error), 0,
              "isNormalizedUTF8 with length -1");
    ExpectInt(error, Success, "isNormalizedUTF8 with length -1 status");

    std::int32_t failing = IllegalArgument;
    ExpectInt(_ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(Nfc(), "abc", 3, &failing), 0,
              "isNormalizedUTF8 with failing status");
    ExpectInt(failing, IllegalArgument, "isNormalizedUTF8 with failing status keeps it");

    error = Success;
    ExpectInt(_ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(Nfc(), nullptr, 3, &error), 0,
              "isNormalizedUTF8 with null data");
    ExpectInt(error, IllegalArgument, "isNormalizedUTF8 with null data status");

    ExpectThrow<std::invalid_argument>(
        [] { _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(Nfc(), "a", 1, nullptr); },
        "isNormalizedUTF8 with null status");
    ExpectThrow<std::invalid_argument>(
        [] {
            std::int32_t status = Success;
            _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(nullptr, "a", 1, &status);
        },
        "isNormalizedUTF8 with null normalizer");
}

template <typename TLookup, typename TReference>
void CompareDecompositions(const Icu::Normalizer2Impl* impl, const Icu::Normalizer2* normalizer, TLookup lookup, TReference reference,
                           const char* what) {
    std::int32_t found = 0;
    for (std::int32_t c = 0; c <= MaxCodePoint; ++c) {
        char16_t expected[Capacity] = {};
        std::int32_t error = Success;
        const auto expectedLength = reference(normalizer, c, expected, Capacity, &error);
        char16_t buffer[30] = {};
        std::int32_t length = -7;
        const auto* actual = lookup(impl, c, buffer, &length);
        if (expectedLength < 0) {
            ExpectAt(actual == nullptr && length == -7, what, c);
            continue;
        }
        ExpectAt(error == Success, what, c);
        ExpectAt(actual != nullptr && length == expectedLength, what, c);
        ExpectAt(std::u16string(actual, length) == std::u16string(expected, expectedLength), what, c);
        ++found;
    }
    Expect(found > 3000, what);
}

void TestDecompositions() {
    const auto getDecomposition = _ZNK6icu_6815Normalizer2Impl16getDecompositionEiPDsRi_nid_postfix;
    const auto getRawDecomposition = _ZNK6icu_6815Normalizer2Impl19getRawDecompositionEiPDsRi_nid_postfix;
    CompareDecompositions(NfkcImpl(), Nfkc(), getDecomposition, unorm2_getDecomposition_nid_postfix, "NFKC getDecomposition");
    CompareDecompositions(NfkcImpl(), Nfkc(), getRawDecomposition, unorm2_getRawDecomposition_nid_postfix, "NFKC getRawDecomposition");
    CompareDecompositions(NfkcCasefoldImpl(), NfkcCasefold(), getDecomposition, unorm2_getDecomposition_nid_postfix,
                          "NFKC_CF getDecomposition");
    CompareDecompositions(NfkcCasefoldImpl(), NfkcCasefold(), getRawDecomposition, unorm2_getRawDecomposition_nid_postfix,
                          "NFKC_CF getRawDecomposition");

    char16_t buffer[30] = {};
    std::int32_t length = 0;
    const auto* hangul = getDecomposition(NfkcImpl(), 0xac01, buffer, &length);
    Expect(hangul == buffer && length == 3 && buffer[0] == 0x1100 && buffer[1] == 0x1161 && buffer[2] == 0x11a8,
           "getDecomposition writes a Hangul syllable to the buffer");
    length = 0;
    const auto* raw = getRawDecomposition(NfkcImpl(), 0xac01, buffer, &length);
    Expect(raw == buffer && length == 2 && buffer[0] == 0xac00 && buffer[1] == 0x11a8,
           "getRawDecomposition writes a Hangul LVT syllable to the buffer");
    ExpectThrow<std::invalid_argument>([&] { getDecomposition(NfkcImpl(), 0xac01, nullptr, &length); },
                                       "getDecomposition with null buffer for Hangul");
    Expect(getDecomposition(NfkcImpl(), 'a', nullptr, &length) == nullptr, "getDecomposition without decomposition ignores buffer");
    ExpectThrow<std::invalid_argument>([&] { getDecomposition(NfkcImpl(), 0xc0, buffer, nullptr); }, "getDecomposition with null length");
    ExpectThrow<std::invalid_argument>([&] { getRawDecomposition(nullptr, 0xc0, buffer, &length); },
                                       "getRawDecomposition with null impl");
}

void TestCompositionAndBoundaries() {
    const auto* impl = NfkcImpl();
    const auto* nfkc = Nfkc();
    const auto* nfkd = Nfkd();
    std::int32_t composites = 0;
    Rng rng{0x0badc0ffee0ddf00ull};
    for (std::int32_t i = 0; i < 400000; ++i) {
        const auto first = i < 200000 ? PickCodePoint(rng) : static_cast<std::int32_t>(rng.Next());
        const auto second = i % 3 == 0 ? static_cast<std::int32_t>(0x300 + rng.Below(0x70)) : PickCodePoint(rng);
        const auto expected = unorm2_composePair_nid_postfix(nfkc, first, second);
        ExpectAt(_ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(impl, first, second) == expected, "Normalizer2Impl::composePair",
                 first);
        if (expected >= 0) ++composites;
    }
    Expect(composites > 1000, "composePair found composites");
    ExpectInt(_ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(impl, 'A', 0x301), 0xc1, "composePair(A, acute)");
    ExpectInt(_ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(impl, 0x1100, 0x1161), 0xac00, "composePair(L, V)");
    ExpectInt(_ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(impl, 0xac00, 0x11a8), 0xac01, "composePair(LV, T)");

    std::int32_t boundaries = 0;
    for (std::int32_t c = -2; c <= MaxCodePoint + 2; ++c) {
        const auto expected = unorm2_hasBoundaryBefore_nid_postfix(nfkd, c) != 0;
        const auto actual = _ZNK6icu_6815Normalizer2Impl23hasDecompBoundaryBeforeEi_nid_postfix(impl, c) != 0;
        ExpectAt(actual == expected, "hasDecompBoundaryBefore matches the NFKD boundary", c);
        if (!actual) ++boundaries;
    }
    Expect(boundaries > 500, "hasDecompBoundaryBefore saw code points without a boundary");

    const auto norm16 = _ZNK6icu_6815Normalizer2Impl29norm16HasDecompBoundaryBeforeEt_nid_postfix;
    Expect(norm16(impl, 0) != 0, "norm16 0 has a boundary");
    Expect(norm16(impl, 0xfc00) != 0, "MIN_NORMAL_MAYBE_YES has a boundary");
    Expect(norm16(impl, 0xfe00) != 0, "JAMO_VT has a boundary");
    Expect(norm16(impl, 0xfe02) == 0, "MIN_YES_YES_WITH_CC has no boundary");
    Expect(norm16(impl, 0xffff) == 0, "the largest norm16 has no boundary");

    ExpectThrow<std::invalid_argument>([] { _ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(nullptr, 'A', 0x301); },
                                       "composePair with null impl");
    ExpectThrow<std::invalid_argument>([] { _ZNK6icu_6815Normalizer2Impl23hasDecompBoundaryBeforeEi_nid_postfix(nullptr, 'A'); },
                                       "hasDecompBoundaryBefore with null impl");
    ExpectThrow<std::invalid_argument>([] { _ZNK6icu_6815Normalizer2Impl29norm16HasDecompBoundaryBeforeEt_nid_postfix(nullptr, 0); },
                                       "norm16HasDecompBoundaryBefore with null impl");
}

void TestCanonIterData() {
    const auto ensure = _ZNK6icu_6815Normalizer2Impl19ensureCanonIterDataER10UErrorCode_nid_postfix;
    for (const auto* impl : {NfkcImpl(), NfkcCasefoldImpl()}) {
        for (std::int32_t round = 0; round < 2; ++round) {
            std::int32_t error = Success;
            Expect(ensure(impl, &error) != 0, "ensureCanonIterData");
            ExpectInt(error, Success, "ensureCanonIterData status");
        }
        std::int32_t failing = IllegalArgument;
        Expect(ensure(impl, &failing) == 0, "ensureCanonIterData with failing status");
        ExpectInt(failing, IllegalArgument, "ensureCanonIterData with failing status keeps it");
        ExpectThrow<std::invalid_argument>([&] { ensure(impl, nullptr); }, "ensureCanonIterData with null status");
    }
    ExpectThrow<std::invalid_argument>(
        [&] {
            std::int32_t error = Success;
            ensure(nullptr, &error);
        },
        "ensureCanonIterData with null impl");
}

void TestLcccChars() {
    const auto addLccc = _ZNK6icu_6815Normalizer2Impl12addLcccCharsERNS_10UnicodeSetE_nid_postfix;
    const auto* nfcImpl = _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(Nfc());
    auto* set = uset_openEmpty_nid_postfix();
    Expect(set != nullptr, "uset_openEmpty");
    uset_add_nid_postfix(set, 'A');
    addLccc(nfcImpl, set);
    std::int32_t members = 0;
    for (std::int32_t c = 0; c <= MaxCodePoint; ++c) {
        const bool expected = c == 'A' || u_getIntPropertyValue_nid_postfix(c, 0x1010) != 0;
        ExpectAt((uset_contains_nid_postfix(set, c) != 0) == expected, "addLcccChars matches Lead_Canonical_Combining_Class", c);
        members += expected;
    }
    Expect(members > 900, "addLcccChars added the code points with a lead combining class");
    const auto count = uset_getItemCount_nid_postfix(set);
    addLccc(nfcImpl, set);
    ExpectInt(uset_getItemCount_nid_postfix(set), count, "addLcccChars twice adds nothing new");
    uset_close_nid_postfix(set);

    auto* compatibility = uset_openEmpty_nid_postfix();
    addLccc(NfkcImpl(), compatibility);
    Expect(uset_contains_nid_postfix(compatibility, 0x301) != 0 && uset_contains_nid_postfix(compatibility, 'a') == 0,
           "addLcccChars with the NFKC data");
    uset_close_nid_postfix(compatibility);

    std::int32_t error = Success;
    const auto* frozen = u_getBinaryPropertySet_nid_postfix(31, &error);
    Expect(frozen != nullptr && error == Success, "White_Space set");
    addLccc(nfcImpl, const_cast<USet*>(frozen));
    Expect(uset_contains_nid_postfix(frozen, 0x301) == 0 && uset_contains_nid_postfix(frozen, ' ') != 0,
           "addLcccChars leaves a frozen property set");
    ExpectThrow<std::invalid_argument>([&] { addLccc(nfcImpl, nullptr); }, "addLcccChars with a null set");
    ExpectThrow<std::invalid_argument>([] {
        auto* empty = uset_openEmpty_nid_postfix();
        try {
            _ZNK6icu_6815Normalizer2Impl12addLcccCharsERNS_10UnicodeSetE_nid_postfix(nullptr, empty);
        } catch (...) {
            uset_close_nid_postfix(empty);
            throw;
        }
    }, "addLcccChars with a null impl");
}

}

int main() {
    TestInstances();
    TestImpls();
    TestBaseMethods();
    TestIsNormalizedUtf8();
    TestDecompositions();
    TestCompositionAndBoundaries();
    TestCanonIterData();
    TestLcccChars();
    std::puts("normalizer2 accessor tests passed");
    return 0;
}
