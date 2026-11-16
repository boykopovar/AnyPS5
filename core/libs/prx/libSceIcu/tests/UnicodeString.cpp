#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using String = Icu::Abi::UnicodeString;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UnicodeString check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, location);
}

String Stack(const std::u16string& text) {
    Require(text.size() <= 27);
    String result{};
    result.storage.stack.lengthAndFlags = static_cast<std::int16_t>((text.size() << 5) | 2);
    std::copy(text.begin(), text.end(), result.storage.stack.buffer);
    return result;
}

void Characters() {
    const char16_t units[]{u'A', 0xd83d, 0xde00, 0xd800, u'B', 0xdc00, 0, 0xdbff, 0xdfff};
    const std::int32_t points[]{u'A', 0x1f600, 0x1f600, 0xd800, u'B', 0xdc00, 0, 0x10ffff, 0x10ffff};
    const std::int32_t starts[]{0, 1, 1, 3, 4, 5, 6, 7, 7};
    const std::int32_t limits[]{0, 1, 3, 3, 4, 5, 6, 7, 9};
    auto stack = Stack(std::u16string(units, 9));
    String alias{};
    alias.storage.fields = {static_cast<std::int16_t>((9 << 5) | 8), 0, 9, const_cast<char16_t*>(units)};
    for (const auto* text : {&stack, &alias}) {
        Require(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(text) == 9);
        Require(_ZNK6icu_6813UnicodeString11hasMetaDataEv_nid_postfix(text) == 0);
        for (int index = 0; index < 9; ++index) {
            Require(_ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, index) == units[index]);
            Require(_ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(text, index) == points[index]);
            Require(_ZNK6icu_6813UnicodeString11getChar32AtEi_nid_postfix(text, index) == points[index]);
            Require(_ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(text, index) == starts[index]);
            Require(_ZNK6icu_6813UnicodeString14getChar32LimitEi_nid_postfix(text, index) == limits[index]);
        }
        for (const auto index : {-1, 9, 10, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            Require(_ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, index) == 0xffff);
            Require(_ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(text, index) == 0xffff);
            Require(_ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(text, index) == 0);
            Require(_ZNK6icu_6813UnicodeString14getChar32LimitEi_nid_postfix(text, index) == 9);
        }
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, 0, 9) == 7);
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, -5, 100) == 7);
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, 2, 6) == 6);
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, 1, 1) == 1);
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, 0, -1) == 0);
        Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(text, 99, 99) == 0);
        for (int number = -1; number <= 10; ++number)
            Require(_ZNK6icu_6813UnicodeString17hasMoreChar32ThanEiii_nid_postfix(text, 0, 99, number) == (number < 7));
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, -1, 2) == 3);
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 100, -2) == 6);
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 2, 1) == 3);
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 2, -1) == 1);
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 4, 100) == 9);
        Require(_ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 4, -100) == 0);
        Invalid([&] { _ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(text, 0, std::numeric_limits<std::int32_t>::min()); });
    }
    Require(_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&stack, &alias));
    Require(_ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(&stack, &alias, 9));
    auto different = stack;
    different.storage.stack.buffer[8] = 0;
    Require(!_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&stack, &different));
    Require(_ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(&stack, &different, 8));
}

void Storage() {
    std::u16string large(1100, u'a');
    String alias{};
    alias.storage.fields = {static_cast<std::int16_t>(0xffe8), 1100, 1100, large.data()};
    Require(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&alias) == 1100);
    Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(&alias, 0, 1100) == 1100);
    auto abc = Stack(u"abc");
    Require(_ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(&abc) == 136518);
    auto empty = Stack(u"");
    Require(_ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(&empty) == 1);
    String bogus{};
    bogus.storage.fields.lengthAndFlags = 1;
    Require(!_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&bogus, &empty));
    Require(_ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&bogus, &bogus));
    Require(_ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(&bogus) == 1);
    Require(_ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(&bogus, 0, 2) == 0);
    Require(_ZNK6icu_6813UnicodeString17hasMoreChar32ThanEiii_nid_postfix(&bogus, 0, 2, -1));
    _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(&bogus);
    Require(bogus.storage.fields.lengthAndFlags == 2);
    _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(&abc);
    Require(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&abc) == 3);
    struct Heap {
        std::int32_t references;
        char16_t units[4];
    } heap{2, {u'a', u'b', u'c', 0}};
    static_assert(offsetof(Heap, units) == 4);
    alias.storage.fields = {static_cast<std::int16_t>((3 << 5) | 4), 0, 4, heap.units};
    Require(_ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(&alias) == 2);
    _ZN6icu_6813UnicodeString6addRefEv_nid_postfix(&alias);
    Require(heap.references == 3);
    Require(_ZN6icu_6813UnicodeString9removeRefEv_nid_postfix(&alias) == 2);
    alias.storage.fields.lengthAndFlags = 4 | 16;
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(&alias, -2);
    Require(alias.storage.fields.lengthAndFlags == (4 | 16));
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(&alias, -1);
    Require(alias.storage.fields.lengthAndFlags == ((3 << 5) | 4));
    abc.storage.fields.lengthAndFlags = 2 | 16;
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(&abc, 100);
    Require(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&abc) == 27);
    alias.storage.fields = {static_cast<std::int16_t>(8 | 16), 0, 1100, large.data()};
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(&alias, 2000);
    Require(alias.storage.fields.lengthAndFlags == static_cast<std::int16_t>(0xffe8));
    Require(alias.storage.fields.length == 1100);
}

void ExtractAndEscape() {
    auto text = Stack(u"abcde");
    std::array<char16_t, 8> buffer;
    buffer.fill(0xa55a);
    _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(&text, -1, 100, buffer.data(), 1);
    Require(std::u16string(buffer.data() + 1, 5) == u"abcde");
    Require(buffer[0] == 0xa55a && buffer[6] == 0xa55a);
    _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(&text, 0, 4, text.storage.stack.buffer, 1);
    Require(std::u16string(text.storage.stack.buffer, 5) == u"aabcd");
    auto escaped = Stack(u"u0041!x{10ffff}u12");
    std::int32_t offset = 0;
    Require(_ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(&escaped, &offset) == 0x41 && offset == 5);
    offset = 6;
    Require(_ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(&escaped, &offset) == 0x10ffff && offset == 15);
    Require(_ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(&escaped, &offset) == -1 && offset == 15);
    Invalid([&] { _ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(&escaped, nullptr); });
    Invalid([&] { _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(&text, 0, 1, nullptr, 0); });
}

void InvalidPointers() {
    Invalid([] { _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(nullptr); });
    Invalid([] { _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(nullptr, 0); });
    Invalid([] { _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(nullptr); });
    Invalid([] { _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(nullptr, 0); });
    String broken{};
    broken.storage.fields.lengthAndFlags = 1 << 5;
    Invalid([&] { _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(&broken, 0); });
    Invalid([&] { _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(&broken); });
    auto text = Stack(u"a");
    Invalid([&] { _ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(&text, &text, -1); });
    Invalid([&] { _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&text, nullptr); });
}

}

int main() {
    Characters();
    Storage();
    ExtractAndEscape();
    InvalidPointers();
    std::puts("ICU UnicodeString inspection tests passed");
}
