#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(&uhash_hashUnicodeString_nid_postfix), std::int32_t (APS5_VABI*)(Icu::Element)>);
static_assert(std::is_same_v<decltype(&uhash_hashCaselessUnicodeString_nid_postfix),
                             std::int32_t (APS5_VABI*)(Icu::Element)>);
static_assert(std::is_same_v<decltype(&uhash_compareUnicodeString_nid_postfix),
                             std::int8_t (APS5_VABI*)(Icu::Element, Icu::Element)>);
static_assert(std::is_same_v<decltype(&uhash_compareCaselessUnicodeString_nid_postfix),
                             std::int8_t (APS5_VABI*)(Icu::Element, Icu::Element)>);
static_assert(sizeof(icu::UnicodeString) == 64);

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU UnicodeString hash check failed at line %u\n", location.line());
    std::abort();
}

struct alignas(8) GuestString {
    unsigned char raw[64]{};
};

GuestString Stack(const std::u16string& text) {
    GuestString object;
    std::memset(object.raw, 0xa5, sizeof(object.raw));
    const auto flags = static_cast<std::int16_t>((text.size() << 5) | 2);
    std::memcpy(object.raw + 8, &flags, sizeof(flags));
    std::memcpy(object.raw + 10, text.data(), text.size() * sizeof(char16_t));
    return object;
}

GuestString Heap(const std::u16string& text, bool large) {
    GuestString object;
    std::memset(object.raw, 0xa5, sizeof(object.raw));
    const auto length = static_cast<std::int32_t>(text.size());
    const auto flags = static_cast<std::int16_t>(large ? 0xffe0 : length << 5);
    std::memcpy(object.raw + 8, &flags, sizeof(flags));
    std::memcpy(object.raw + 12, &length, sizeof(length));
    const char16_t* array = text.data();
    std::memcpy(object.raw + 24, &array, sizeof(array));
    return object;
}

GuestString Bogus() {
    GuestString object;
    const std::int16_t flags = 1;
    std::memcpy(object.raw + 8, &flags, sizeof(flags));
    return object;
}

Icu::Element Key(const void* pointer) {
    Icu::Element element{};
    element.pointer = const_cast<void*>(pointer);
    return element;
}

icu::UnicodeString Native(const std::u16string& text) {
    return icu::UnicodeString(text.data(), static_cast<std::int32_t>(text.size()));
}

void CheckHashes(const void* object, const icu::UnicodeString& expected) {
    icu::UnicodeString folded(expected);
    Require(uhash_hashUnicodeString_nid_postfix(Key(object)) == expected.hashCode());
    Require(uhash_hashCaselessUnicodeString_nid_postfix(Key(object)) == folded.foldCase().hashCode());
}

template <typename Action>
void RequireThrows(Action action) {
    errno = 71;
    try {
        action();
    } catch (const std::exception&) {
        Require(errno == 71);
        return;
    }
    Require(false);
}

}

int main() {
    const std::vector<std::u16string> texts = {
        u"", u"a", u"Hello", u"HELLO", u"hello", u"Straße", u"STRASSE", u"Σίσυφος",
        u"exactly twenty seven chars", u"a heap string longer than the stack buffer", std::u16string(1100, u'Z')};

    for (const auto& text : texts) {
        const auto expected = Native(text);
        CheckHashes(&expected, expected);
        if (text.size() <= 27) {
            const auto stack = Stack(text);
            CheckHashes(&stack, expected);
        }
        if (text.size() <= 0x3ff) {
            const auto heap = Heap(text, false);
            CheckHashes(&heap, expected);
        }
        const auto large = Heap(text, true);
        CheckHashes(&large, expected);
    }
    icu::UnicodeString nativeBogus;
    nativeBogus.setToBogus();
    const auto bogus = Bogus();
    CheckHashes(&bogus, nativeBogus);
    CheckHashes(&nativeBogus, nativeBogus);
    Require(uhash_hashUnicodeString_nid_postfix(Key(nullptr)) == 0);
    Require(uhash_hashCaselessUnicodeString_nid_postfix(Key(nullptr)) == 0);

    std::vector<icu::UnicodeString> natives;
    for (const auto& text : texts) natives.push_back(Native(text));
    natives.push_back(nativeBogus);
    for (const auto& left : natives) {
        for (const auto& right : natives) {
            const bool equal = left == right;
            const bool caseless = left.caseCompare(right, U_FOLD_CASE_DEFAULT) == 0;
            Require(uhash_compareUnicodeString_nid_postfix(Key(&left), Key(&right)) == equal);
            Require(uhash_compareCaselessUnicodeString_nid_postfix(Key(&left), Key(&right)) == caseless);
            icu::UnicodeString copy(left);
            Require(uhash_compareUnicodeString_nid_postfix(Key(&copy), Key(&right)) == equal);
            Require(uhash_compareCaselessUnicodeString_nid_postfix(Key(&copy), Key(&right)) == caseless);
        }
    }
    const auto hello = Stack(u"Hello");
    const std::u16string upperText = u"HELLO";
    const auto upper = Heap(upperText, false);
    Require(uhash_compareUnicodeString_nid_postfix(Key(&hello), Key(&upper)) == 0);
    Require(uhash_compareCaselessUnicodeString_nid_postfix(Key(&hello), Key(&upper)) == 1);
    Require(uhash_compareUnicodeString_nid_postfix(Key(&bogus), Key(&nativeBogus)) == 1);
    Require(uhash_compareUnicodeString_nid_postfix(Key(&bogus), Key(&hello)) == 0);
    for (const auto* object : {static_cast<const void*>(&hello), static_cast<const void*>(nullptr)}) {
        Require(uhash_compareUnicodeString_nid_postfix(Key(object), Key(object)) == 1);
        Require(uhash_compareCaselessUnicodeString_nid_postfix(Key(object), Key(object)) == 1);
    }
    Require(uhash_compareUnicodeString_nid_postfix(Key(&hello), Key(nullptr)) == 0);
    Require(uhash_compareCaselessUnicodeString_nid_postfix(Key(nullptr), Key(&hello)) == 0);

    const auto* invalid = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    RequireThrows([&] { uhash_hashUnicodeString_nid_postfix(Key(invalid)); });
    RequireThrows([&] { uhash_hashCaselessUnicodeString_nid_postfix(Key(invalid)); });
    RequireThrows([&] { uhash_compareUnicodeString_nid_postfix(Key(&hello), Key(invalid)); });
    RequireThrows([&] { uhash_compareCaselessUnicodeString_nid_postfix(Key(invalid), Key(&hello)); });
    auto dangling = Heap(u"abc", false);
    const auto* bad = invalid;
    std::memcpy(dangling.raw + 24, &bad, sizeof(bad));
    RequireThrows([&] { uhash_hashUnicodeString_nid_postfix(Key(&dangling)); });
    auto negative = Heap(u"abc", true);
    const std::int32_t minusOne = -1;
    std::memcpy(negative.raw + 12, &minusOne, sizeof(minusOne));
    RequireThrows([&] { uhash_hashUnicodeString_nid_postfix(Key(&negative)); });
    auto huge = Heap(u"abc", true);
    const std::int32_t maxLength = std::numeric_limits<std::int32_t>::max();
    std::memcpy(huge.raw + 12, &maxLength, sizeof(maxLength));
    std::memcpy(huge.raw + 24, &bad, sizeof(bad));
    RequireThrows([&] { uhash_hashUnicodeString_nid_postfix(Key(&huge)); });
    RequireThrows([&] { uhash_compareCaselessUnicodeString_nid_postfix(Key(&hello), Key(&huge)); });
}
