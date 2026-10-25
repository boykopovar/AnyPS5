#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <string>


namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU search check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
void Throws(TCall call) {
    try { call(); }
    catch (const std::exception&) { return; }
    Require(false);
}

void Memory() {
    std::array<char16_t, 20> original;
    for (std::size_t index = 0; index < original.size(); ++index) original[index] = static_cast<char16_t>(0x1234 + index * 257);
    for (std::int32_t count = 0; count <= 10; ++count) {
        for (std::size_t source = 1; source <= 8; ++source) {
            for (std::size_t destination = 1; destination <= 8; ++destination) {
                auto actual = original;
                auto expected = original;
                for (std::int32_t offset = 0; offset < count; ++offset) expected[destination + offset] = original[source + offset];
                Require(u_memmove_nid_postfix(actual.data() + destination, actual.data() + source, count) == actual.data() + destination);
                Require(actual == expected);
            }
        }
        for (const char16_t value : std::array<char16_t, 4>{0, 0xffff, 0x1234, 0xd800}) {
            auto actual = original;
            auto expected = original;
            for (std::int32_t offset = 0; offset < count; ++offset) expected[3 + offset] = value;
            Require(u_memset_nid_postfix(actual.data() + 3, value, count) == actual.data() + 3);
            Require(actual == expected);
        }
    }
    for (const auto count : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        Require(u_memmove_nid_postfix(nullptr, nullptr, count) == nullptr);
        Require(u_memset_nid_postfix(nullptr, 0xffff, count) == nullptr);
        Require(u_memmove_nid_postfix(original.data(), nullptr, count) == original.data());
    }
    Throws([&] { u_memmove_nid_postfix(nullptr, original.data(), 1); });
    Throws([&] { u_memmove_nid_postfix(original.data(), nullptr, 1); });
    Throws([&] { u_memset_nid_postfix(nullptr, 0, 1); });
}

void Characters() {
    const std::array<char16_t, 8> text{0xd800, 0xdc00, 'a', 0xd800, 'b', 0xdc00, 'a', 0};
    const auto original = text;
    Require(u_strchr_nid_postfix(text.data(), 'a') == text.data() + 2);
    Require(u_strrchr_nid_postfix(text.data(), 'a') == text.data() + 6);
    Require(u_strchr_nid_postfix(text.data(), 0xd800) == text.data() + 3);
    Require(u_strrchr_nid_postfix(text.data(), 0xdc00) == text.data() + 5);
    Require(u_strchr_nid_postfix(text.data(), 0) == text.data() + 7);
    Require(u_strrchr_nid_postfix(text.data(), 0) == text.data() + 7);
    Require(u_strchr_nid_postfix(text.data(), 0xffff) == nullptr);
    Require(u_memchr_nid_postfix(text.data(), 0xd800, 1) == text.data());
    Require(u_memrchr_nid_postfix(text.data(), 0xd800, 2) == nullptr);
    Require(u_memchr_nid_postfix(text.data() + 1, 0xdc00, 1) == text.data() + 1);
    Require(u_memrchr_nid_postfix(text.data(), 'a', 8) == text.data() + 6);
    const std::array<char16_t, 5> embedded{'a', 0, 'b', 0, 'a'};
    Require(u_memchr_nid_postfix(embedded.data(), 'b', 5) == embedded.data() + 2);
    Require(u_memrchr_nid_postfix(embedded.data(), 0, 5) == embedded.data() + 3);
    for (const auto count : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        Require(u_memchr_nid_postfix(nullptr, 'a', count) == nullptr);
        Require(u_memrchr_nid_postfix(nullptr, 'a', count) == nullptr);
    }
    Require(u_memchr_nid_postfix(nullptr, 0xd800, 7) == nullptr);
    Require(u_memrchr_nid_postfix(nullptr, 0xdfff, 7) == nullptr);
    Require(u_strchr_nid_postfix(nullptr, 0xd800) == nullptr);
    Require(u_strrchr_nid_postfix(nullptr, 0xdfff) == nullptr);
    Throws([&] { u_memchr_nid_postfix(nullptr, 'a', 1); });
    Throws([&] { u_memrchr_nid_postfix(nullptr, 'a', 1); });
    Throws([&] { u_strchr_nid_postfix(nullptr, 'a'); });
    Throws([&] { u_strrchr_nid_postfix(nullptr, 'a'); });
    Require(text == original);
}

void Substrings() {
    const char16_t text[]{'a', 'b', 'a', 'b', 'a', 0};
    for (const auto find : {u_strFindFirst_nid_postfix, u_strFindLast_nid_postfix}) {
        const auto last = find == u_strFindLast_nid_postfix;
        Require(find(text, -1, u"aba", -1) == text + (last ? 2 : 0));
        Require(find(text, 4, u"aba", 3) == text);
        Require(find(text, -1, u"", -1) == text);
        Require(find(text, -1, u"A", -1) == nullptr);
        Require(find(text, -2, nullptr, -1) == text);
        Require(find(text, -2, u"", 0) == nullptr);
        Require(find(nullptr, 0, u"", 0) == nullptr);
        Require(find(text, 0, text, -2) == text);
        const char16_t nul[]{0};
        Require(find(text, -1, nul, 1) == text + 5);
        Require(find(text, 5, nul, 1) == nullptr);
        const std::array<char16_t, 5> embedded{'a', 0, 'a', 0, 'b'};
        const char16_t needle[]{'a', 0};
        Require(find(embedded.data(), 5, needle, 2) == embedded.data() + (last ? 2 : 0));
        Require(find(embedded.data(), -1, needle, 2) == nullptr);
        const char16_t pair[]{'a', 0xd800, 0xdc00, 'b', 0};
        const char16_t partial[]{'a', 0xd800};
        Require(find(pair, 4, partial, 2) == nullptr);
        Require(find(pair, 2, partial, 2) == pair);
        Require(find(pair + 2, 1, pair + 2, 1) == pair + 2);
        auto* unread = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
        Require(find(unread, 0, unread, 0) == unread);
    }
    std::u16string longText(100000, u'x');
    longText[0] = u'a';
    longText[1] = u'b';
    longText[99999] = u'a';
    Require(u_strFindFirst_nid_postfix(longText.data(), -1, u"ab", -1) == longText.data());
    Require(u_strFindFirst_nid_postfix(longText.data(), -1, u"ab", 2) == longText.data());
    Require(u_strFindLast_nid_postfix(longText.data(), -1, u"a", -1) == longText.data() + 99999);
    std::u16string longNeedle(100000, u'a');
    Require(u_strFindFirst_nid_postfix(u"", -1, longNeedle.data(), -1) == nullptr);
    Require(u_strFindFirst_nid_postfix(u"b", -1, longNeedle.data(), -1) == nullptr);
}

void Comparisons() {
    const char16_t supplementary[]{0xd800, 0xdc00, 0};
    const char16_t bmp[]{0xe000, 0, 0};
    Require(u_strcmpCodePointOrder_nid_postfix(supplementary, bmp) == 0x2000);
    Require(u_strncmpCodePointOrder_nid_postfix(supplementary, bmp, 1) == -0x800);
    Require(u_strncmpCodePointOrder_nid_postfix(supplementary, bmp, 2) == 0x2000);
    Require(u_strcmpCodePointOrder_nid_postfix(u"a", u"ab") == -98);
    Require(u_strncmpCodePointOrder_nid_postfix(u"a", u"ab", 1) == 0);
    for (const auto count : {-1, -2, std::numeric_limits<std::int32_t>::min()})
        Require(u_strncmpCodePointOrder_nid_postfix(u"a", u"ab", count) == -98);
    for (const auto count : {0, -1, 1, std::numeric_limits<std::int32_t>::max()})
        Require(u_strncmpCodePointOrder_nid_postfix(nullptr, nullptr, count) == 0);
    Require(u_strncmpCodePointOrder_nid_postfix(nullptr, u"a", 0) == 0);
    Require(u_strcmpCodePointOrder_nid_postfix(nullptr, nullptr) == 0);
    Throws([&] { u_strcmpCodePointOrder_nid_postfix(nullptr, u"a"); });
    Throws([&] { u_strncmpCodePointOrder_nid_postfix(u"a", nullptr, -1); });
}

} // namespace

int main() {
    Memory();
    Characters();
    Substrings();
    Comparisons();
}
