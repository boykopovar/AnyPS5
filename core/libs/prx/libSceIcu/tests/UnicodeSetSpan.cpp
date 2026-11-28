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
constexpr std::int32_t NotContained = 0;
constexpr std::int32_t Contained = 1;
constexpr std::int32_t Simple = 2;
constexpr std::int32_t MaxCodePoint = 0x10ffff;
constexpr std::int32_t GeneralCategory = 0x1005;
constexpr std::int32_t GeneralCategoryMask = 0x2000;
constexpr std::int32_t BinaryAlphabetic = 0;
constexpr std::int32_t BinaryWhiteSpace = 31;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectAt(bool condition, const char* what, std::int64_t value, std::int64_t detail) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s at %lld (%lld)\n", what, static_cast<long long>(value), static_cast<long long>(detail));
    std::abort();
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

struct OwnedSet {
    USet* set = uset_openEmpty_nid_postfix();

    OwnedSet() {
        Expect(set != nullptr, "uset_openEmpty");
    }

    ~OwnedSet() {
        uset_close_nid_postfix(set);
    }

    OwnedSet(const OwnedSet&) = delete;
    OwnedSet& operator=(const OwnedSet&) = delete;
};

bool Contains(const USet* set, std::int32_t c) {
    return uset_contains_nid_postfix(set, c) != 0;
}

struct Unit {
    std::int32_t codePoint;
    std::int32_t begin;
    std::int32_t end;
};

std::vector<Unit> DecodeUtf16(const std::u16string& text) {
    std::vector<Unit> units;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(text.size());) {
        const auto begin = i;
        std::int32_t c = text[i++];
        if (c >= 0xd800 && c <= 0xdbff && i < static_cast<std::int32_t>(text.size()) && text[i] >= 0xdc00 && text[i] <= 0xdfff) {
            c = 0x10000 + ((c - 0xd800) << 10) + (text[i++] - 0xdc00);
        }
        units.push_back({c, begin, i});
    }
    return units;
}

std::vector<Unit> DecodeUtf8(const std::string& text, const std::vector<std::int32_t>& codePoints) {
    std::vector<Unit> units;
    std::int32_t offset = 0;
    for (const auto c : codePoints) {
        const std::int32_t size = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        units.push_back({c, offset, offset + size});
        offset += size;
    }
    Expect(offset == static_cast<std::int32_t>(text.size()), "UTF-8 reference length");
    return units;
}

std::int32_t ForwardSpan(const USet* set, const std::vector<Unit>& units, std::int32_t condition) {
    const bool wanted = condition != NotContained;
    for (const auto& unit : units) {
        if (Contains(set, unit.codePoint) != wanted) return unit.begin;
    }
    return units.empty() ? 0 : units.back().end;
}

std::int32_t BackwardSpan(const USet* set, const std::vector<Unit>& units, std::int32_t condition) {
    const bool wanted = condition != NotContained;
    for (auto it = units.rbegin(); it != units.rend(); ++it) {
        if (Contains(set, it->codePoint) != wanted) return it->end;
    }
    return 0;
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

std::int32_t Pick(Rng& rng, const std::vector<std::int32_t>& members) {
    switch (rng.Below(4)) {
    case 0:
    case 1:
        return members[rng.Below(static_cast<std::uint32_t>(members.size()))];
    case 2:
        return static_cast<std::int32_t>(rng.Below(0x300));
    default: {
        const auto c = static_cast<std::int32_t>(rng.Below(MaxCodePoint + 1));
        return c >= 0xd800 && c <= 0xdfff ? 0x10437 : c;
    }
    }
}

void TestSpans() {
    OwnedSet owned;
    uset_addRange_nid_postfix(owned.set, 'a', 'z');
    uset_addRange_nid_postfix(owned.set, 0x3040, 0x309f);
    uset_add_nid_postfix(owned.set, 0x10437);
    uset_add_nid_postfix(owned.set, 0x1f600);
    uset_add_nid_postfix(owned.set, 0xe9);
    std::int32_t error = Success;
    const auto* whiteSpace = u_getBinaryPropertySet_nid_postfix(BinaryWhiteSpace, &error);
    Expect(whiteSpace != nullptr && error == Success, "u_getBinaryPropertySet(White_Space)");
    const std::vector<std::int32_t> members = {'a', 'q', 'z', 0x3042, 0x309f, 0x10437, 0x1f600, 0xe9, ' ', 0x3000, 0x2028, 'A', 0x10438};

    Rng rng{0x51ced5eed5eedull};
    for (const USet* set : {static_cast<const USet*>(owned.set), whiteSpace}) {
        for (std::int32_t round = 0; round < 20000; ++round) {
            std::vector<std::int32_t> codePoints;
            const auto count = rng.Below(12);
            for (std::uint32_t i = 0; i < count; ++i) codePoints.push_back(Pick(rng, members));
            std::u16string utf16;
            std::string utf8;
            for (const auto c : codePoints) {
                AppendUtf16(utf16, c);
                AppendUtf8(utf8, c);
            }
            if (rng.Below(8) == 0) utf16 += static_cast<char16_t>(0xd800 + rng.Below(0x800));
            const auto units16 = DecodeUtf16(utf16);
            const auto units8 = DecodeUtf8(utf8, codePoints);
            const auto length16 = static_cast<std::int32_t>(utf16.size());
            const auto length8 = static_cast<std::int32_t>(utf8.size());
            for (const auto condition : {NotContained, Contained, Simple}) {
                ExpectAt(_ZNK6icu_6810UnicodeSet4spanEPKDsi17USetSpanCondition_nid_postfix(set, utf16.data(), length16, condition) ==
                             ForwardSpan(set, units16, condition),
                         "span", round, condition);
                ExpectAt(_ZNK6icu_6810UnicodeSet8spanBackEPKDsi17USetSpanCondition_nid_postfix(set, utf16.data(), length16, condition) ==
                             BackwardSpan(set, units16, condition),
                         "spanBack", round, condition);
                ExpectAt(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(set, utf8.data(), length8, condition) ==
                             ForwardSpan(set, units8, condition),
                         "spanUTF8", round, condition);
                ExpectAt(_ZNK6icu_6810UnicodeSet12spanBackUTF8EPKci17USetSpanCondition_nid_postfix(set, utf8.data(), length8,
                                                                                                    condition) ==
                             BackwardSpan(set, units8, condition),
                         "spanBackUTF8", round, condition);
                if (utf16.find(char16_t{0}) == std::u16string::npos) {
                    ExpectAt(_ZNK6icu_6810UnicodeSet4spanEPKDsi17USetSpanCondition_nid_postfix(set, utf16.c_str(), -1, condition) ==
                                 ForwardSpan(set, units16, condition),
                             "span with length -1", round, condition);
                }
                if (utf8.find('\0') == std::string::npos) {
                    ExpectAt(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(set, utf8.c_str(), -1, condition) ==
                                 ForwardSpan(set, units8, condition),
                             "spanUTF8 with length -1", round, condition);
                }
            }
        }
    }

    OwnedSet replacement;
    uset_addRange_nid_postfix(replacement.set, 'a', 'b');
    uset_add_nid_postfix(replacement.set, 0xfffd);
    const std::string illFormed = "a\x80" "b";
    Expect(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(replacement.set, illFormed.data(), 3, Contained) == 3,
           "spanUTF8 reads an ill-formed byte as U+FFFD");
    Expect(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, illFormed.data(), 3, Contained) == 1,
           "spanUTF8 stops at an ill-formed byte outside the set");
    Expect(_ZNK6icu_6810UnicodeSet12spanBackUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, illFormed.data(), 3, Contained) == 2,
           "spanBackUTF8 stops at an ill-formed byte outside the set");
    const std::string truncated = "\xe3\x81";
    Expect(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(replacement.set, truncated.data(), 2, Contained) == 2,
           "spanUTF8 reads a truncated sequence as one U+FFFD");
    Expect(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, truncated.data(), 2, NotContained) == 2,
           "spanUTF8 NOT_CONTAINED over a truncated sequence");

    Expect(_ZNK6icu_6810UnicodeSet4spanEPKDsi17USetSpanCondition_nid_postfix(owned.set, nullptr, 0, Contained) == 0, "span(null, 0)");
    Expect(_ZNK6icu_6810UnicodeSet8spanBackEPKDsi17USetSpanCondition_nid_postfix(owned.set, nullptr, 0, Contained) == 0,
           "spanBack(null, 0)");
    Expect(_ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, nullptr, 0, Contained) == 0,
           "spanUTF8(null, 0)");
    Expect(_ZNK6icu_6810UnicodeSet12spanBackUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, nullptr, 0, Contained) == 0,
           "spanBackUTF8(null, 0)");
    ExpectInvalid([&] { _ZNK6icu_6810UnicodeSet4spanEPKDsi17USetSpanCondition_nid_postfix(owned.set, nullptr, 2, Contained); },
                  "span(null, 2)");
    ExpectInvalid([&] { _ZNK6icu_6810UnicodeSet8spanUTF8EPKci17USetSpanCondition_nid_postfix(owned.set, nullptr, -1, Contained); },
                  "spanUTF8(null, -1)");
    ExpectInvalid([] { _ZNK6icu_6810UnicodeSet4spanEPKDsi17USetSpanCondition_nid_postfix(nullptr, u"a", 1, Contained); },
                  "span(null set)");
    ExpectInvalid([] { _ZNK6icu_6810UnicodeSet12spanBackUTF8EPKci17USetSpanCondition_nid_postfix(nullptr, "a", 1, Contained); },
                  "spanBackUTF8(null set)");
    const std::int32_t notASet[16] = {};
    ExpectInvalid([&] {
        _ZNK6icu_6810UnicodeSet8spanBackEPKDsi17USetSpanCondition_nid_postfix(reinterpret_cast<const USet*>(notASet), u"a", 1, Contained);
    }, "spanBack on memory that is not a set");
}

std::int32_t CompareProperty(std::int32_t property, std::int32_t value) {
    OwnedSet owned;
    uset_add_nid_postfix(owned.set, 0x10ffff);
    std::int32_t error = Success;
    auto* result = _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(owned.set, property, value, &error);
    Expect(result == owned.set, "applyIntPropertyValue returns its set");
    ExpectAt(error == Success, "applyIntPropertyValue status", property, error);
    std::int32_t members = 0;
    for (std::int32_t c = 0; c <= MaxCodePoint; ++c) {
        const auto actual = u_getIntPropertyValue_nid_postfix(c, property);
        const bool expected = property == GeneralCategoryMask ? ((1u << static_cast<std::uint32_t>(u_getIntPropertyValue_nid_postfix(c, GeneralCategory))) &
                                                                 static_cast<std::uint32_t>(value)) != 0
                                                              : actual == value;
        ExpectAt(Contains(owned.set, c) == expected, "applyIntPropertyValue membership", property * 0x100000000ll + value, c);
        members += expected;
    }
    return members;
}

void TestApplyIntPropertyValue() {
    const std::int32_t properties[] = {0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1008, 0x1009, 0x100a, 0x100b,
                                       0x100c, 0x100d, 0x100e, 0x100f, 0x1010, 0x1011, 0x1012, 0x1013, 0x1014, 0x1015};
    for (const auto property : properties) {
        const auto maximum = u_getIntPropertyMaxValue_nid_postfix(property);
        Expect(maximum > 0, "u_getIntPropertyMaxValue");
        std::int32_t members = 0;
        for (const auto value : {std::int32_t{0}, std::int32_t{1}, maximum}) members += CompareProperty(property, value);
        ExpectAt(members > 0, "applyIntPropertyValue selected code points", property, maximum);
    }
    Expect(CompareProperty(BinaryAlphabetic, 1) > 1000, "applyIntPropertyValue(Alphabetic, 1)");
    Expect(CompareProperty(BinaryWhiteSpace, 0) > 1000, "applyIntPropertyValue(White_Space, 0)");
    Expect(CompareProperty(GeneralCategoryMask, (1 << 1) | (1 << 9)) > 1000, "applyIntPropertyValue(General_Category_Mask)");

    OwnedSet owned;
    uset_add_nid_postfix(owned.set, 'x');
    std::int32_t error = Success;
    _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(owned.set, 0x9999, 1, &error);
    Expect(error == IllegalArgument, "applyIntPropertyValue with an unknown property");
    _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(owned.set, BinaryAlphabetic, 2, &(error = Success));
    Expect(error == Success && uset_getItemCount_nid_postfix(owned.set) == 0, "applyIntPropertyValue with a binary value above 1 empties");

    uset_add_nid_postfix(owned.set, 'x');
    std::int32_t failing = IllegalArgument;
    _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(owned.set, GeneralCategory, 1, &failing);
    Expect(failing == IllegalArgument && Contains(owned.set, 'x') && uset_getItemCount_nid_postfix(owned.set) == 1,
           "applyIntPropertyValue with a failing status leaves the set");

    const auto* borrowed = u_getBinaryPropertySet_nid_postfix(BinaryWhiteSpace, &(error = Success));
    auto* mutableBorrowed = const_cast<USet*>(borrowed);
    Expect(_ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(mutableBorrowed, GeneralCategory, 1, &error) ==
               mutableBorrowed,
           "applyIntPropertyValue on a frozen property set returns it");
    Expect(error == Success && Contains(borrowed, ' ') && !Contains(borrowed, 'A'), "applyIntPropertyValue leaves a frozen set");

    ExpectInvalid([&] { _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(owned.set, GeneralCategory, 1, nullptr); },
                  "applyIntPropertyValue with null status");
    ExpectInvalid([&] { _ZN6icu_6810UnicodeSet21applyIntPropertyValueE9UPropertyiR10UErrorCode_nid_postfix(nullptr, GeneralCategory, 1, &error); },
                  "applyIntPropertyValue with null set");
}

}

int main() {
    TestSpans();
    TestApplyIntPropertyValue();
    std::puts("unicode set span tests passed");
    return 0;
}
