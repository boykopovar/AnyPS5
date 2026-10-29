#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

void require(bool condition) {
    if (!condition) std::abort();
}

bool same(const char16_t* left, const char16_t* right, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (left[index] != right[index]) return false;
    }
    return true;
}

}

int main() {
    {
        const auto bytesOf = [](const char* text, std::size_t count) { return std::string(text, count); };
        char out[8] = {};
        require(c32rtomb_nid_postfix(out, 0x41, nullptr) == 1 && out[0] == 'A');
        require(c32rtomb_nid_postfix(out, 0xe9, nullptr) == 2 && bytesOf(out, 2) == "\xc3\xa9");
        require(c32rtomb_nid_postfix(out, 0x20ac, nullptr) == 3 && bytesOf(out, 3) == "\xe2\x82\xac");
        require(c32rtomb_nid_postfix(out, 0x1f600, nullptr) == 4 && bytesOf(out, 4) == "\xf0\x9f\x98\x80");
        require(c32rtomb_nid_postfix(out, 0x7fffffff, nullptr) == 6 && bytesOf(out, 6) == "\xfd\xbf\xbf\xbf\xbf\xbf");
        require(c32rtomb_nid_postfix(nullptr, 0x20ac, nullptr) == 1);

        alignas(8) unsigned char state[16] = {};
        require(c16rtomb_nid_postfix(out, 0xd83d, state) == 0 && state[10] == 1);
        require(c16rtomb_nid_postfix(out, 0x41, state) == static_cast<std::size_t>(-1) && state[10] == 1);
        require(c16rtomb_nid_postfix(nullptr, 0x41, state) == static_cast<std::size_t>(-1));
        require(c16rtomb_nid_postfix(out, 0xde00, state) == 4 && bytesOf(out, 4) == "\xf0\x9f\x98\x80" && state[10] == 0);
        require(c16rtomb_nid_postfix(out, 0xdc00, state) == 3 && bytesOf(out, 3) == "\xed\xb0\x80");
        require(c16rtomb_nid_postfix(out, 0x7f, state) == 1 && out[0] == 0x7f);

        std::memset(state, 0, sizeof(state));
        std::uint32_t wide = 0;
        require(mbrtoc32_nid_postfix(&wide, "\xf0\x9f\x98\x80z", 5, state) == 4 && wide == 0x1f600);
        require(mbrtoc32_nid_postfix(&wide, "\xe2\x82", 2, state) == static_cast<std::size_t>(-2) && state[10] == 1);
        require(mbrtoc32_nid_postfix(&wide, "\xac", 1, state) == 1 && wide == 0x20ac && state[10] == 0);
        require(mbrtoc32_nid_postfix(&wide, "", 1, state) == 0 && wide == 0);
        require(mbrtoc32_nid_postfix(&wide, "A", 0, state) == static_cast<std::size_t>(-2));
        wide = 7;
        require(mbrtoc32_nid_postfix(&wide, nullptr, 9, state) == 0 && wide == 7);
        *__error_nid_postfix() = 0;
        require(mbrtoc32_nid_postfix(&wide, "\x80", 1, state) == static_cast<std::size_t>(-1) && *__error_nid_postfix() == 86);
        require(mbrtoc32_nid_postfix(&wide, "\xfd\xbf\xbf\xbf\xbf\xbf", 6, state) == 6 && wide == 0x7fffffff);

        std::memset(state, 0, sizeof(state));
        std::uint16_t unit = 0;
        require(mbrtoc16_nid_postfix(&unit, "\xc3\xa9", 2, state) == 2 && unit == 0xe9);
        require(mbrtoc16_nid_postfix(&unit, "\xf0\x9f", 2, state) == static_cast<std::size_t>(-2));
        require(mbrtoc16_nid_postfix(&unit, "\x98\x80", 2, state) == 2 && unit == 0xd83d && state[10] == 0x10);
        require(mbrtoc16_nid_postfix(&unit, "A", 1, state) == static_cast<std::size_t>(-3) && unit == 0xdc1f && state[10] == 0);
        std::memset(state, 0, sizeof(state));
        require(mbrtoc16_nid_postfix(&unit, "\xf0\x9f\x98\x80", 4, state) == 4 && unit == 0xd83d);
        require(mbrtoc16_nid_postfix(&unit, nullptr, 0, state) == static_cast<std::size_t>(-3) && unit == 0xd83d);
        *__error_nid_postfix() = 0;
        require(mbrtoc16_nid_postfix(&unit, "\xf4\x90\x80\x80", 4, state) == static_cast<std::size_t>(-1) && *__error_nid_postfix() == 86);

        require(mblen_nid_postfix(nullptr, 1) == 0 && mblen_nid_postfix("ab", 2) == 1 && mblen_nid_postfix("", 1) == 0);
        require(wctomb_nid_postfix(nullptr, 0x41) == 0);
        require(wctomb_nid_postfix(out, 0x42) == 1 && out[0] == 'B');
        require(wctomb_nid_postfix(out, 0x263a) == 3 && std::memcmp(out, "\xe2\x98\xba", 3) == 0);
    }
    const char16_t split[] = {u'a', u'b', 0, u'c', 0};
    require(wcslen_nid_postfix(u"") == 0);
    require(wcslen_nid_postfix(u"héllo") == 5);
    require(wcslen_nid_postfix(split) == 2);

    require(wcsnlen_s_nid_postfix(nullptr, 10) == 0);
    require(wcsnlen_s_nid_postfix(u"", 5) == 0);
    require(wcsnlen_s_nid_postfix(u"héllo", 0) == 0);
    require(wcsnlen_s_nid_postfix(u"héllo", 3) == 3);
    require(wcsnlen_s_nid_postfix(u"héllo", 10) == 5);
    require(wcsnlen_s_nid_postfix(split, 4) == 2);

    require(wcscmp_nid_postfix(u"abc", u"abc") == 0);
    require(wcscmp_nid_postfix(u"abc", u"abd") < 0);
    require(wcscmp_nid_postfix(u"abd", u"abc") > 0);
    require(wcscmp_nid_postfix(u"ab", u"abc") < 0);
    require(wcscmp_nid_postfix(u"￿", u"a") > 0);
    require(wcscmp_nid_postfix(u"耀", u"翿") > 0);
    require(wcsncmp_nid_postfix(u"abcx", u"abcy", 3) == 0);
    require(wcsncmp_nid_postfix(u"abcx", u"abcy", 4) < 0);
    require(wcsncmp_nid_postfix(u"ab", u"ab", 10) == 0);
    require(wcsncmp_nid_postfix(u"a", u"b", 0) == 0);

    char16_t buffer[8];
    wmemset_nid_postfix(buffer, 0xaaaa, 8);

    require(wcscpy_nid_postfix(buffer, u"wide") == buffer);
    require(same(buffer, u"wide", 5) && buffer[5] == 0xaaaa);

    char16_t concatenated[8];
    wmemset_nid_postfix(concatenated, 0xaaaa, 8);

    concatenated[0] = 0;
    require(wcscat_nid_postfix(concatenated, u"ab") == concatenated);
    require(wcscat_nid_postfix(concatenated, u"世") == concatenated);
    const char16_t expectedConcatenated[] = {u'a', u'b', u'世', 0, 0xaaaa};
    require(same(concatenated, expectedConcatenated, 5));

    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    require(wcsncpy_nid_postfix(buffer, u"ab", 5) == buffer);
    const char16_t padded[] = {u'a', u'b', 0, 0, 0, 0xaaaa};
    require(same(buffer, padded, 6));
    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    wcsncpy_nid_postfix(buffer, u"abcdef", 3);

    const char16_t truncated[] = {u'a', u'b', u'c', 0xaaaa};
    require(same(buffer, truncated, 4));

    char16_t memBuf[16];
    require(wmemcpy_s_nid_postfix(memBuf, 16, u"test", 4) == 0 && same(memBuf, u"test", 4));
    require(wmemcpy_s_nid_postfix(memBuf, 16, u"abc", 0) == 0);
    require(wmemcpy_s_nid_postfix(nullptr, 16, u"abc", 3) == 22);
    require(wmemcpy_s_nid_postfix(memBuf, 16, nullptr, 3) == 22 && memBuf[0] == 0);
    wmemset_nid_postfix(memBuf, u'Z', 16);

    require(wmemcpy_s_nid_postfix(memBuf, 4, u"12345", 5) == 34 && memBuf[0] == 0 && memBuf[3] == 0);
    require(wmemcpy_s_nid_postfix(memBuf, ((SIZE_MAX >> 1) / sizeof(char16_t)) + 1, u"a", 1) == 34);
    wmemcpy_s_nid_postfix(memBuf, 16, u"abcdefgh", 8);

    require(wmemcpy_s_nid_postfix(memBuf + 2, 8, memBuf, 4) == 22 && memBuf[2] == 0);

    wmemcpy_s_nid_postfix(memBuf, 16, u"0123456789", 10);
    require(wmemmove_s_nid_postfix(memBuf + 2, 10, memBuf, 5) == 0);
    require(same(memBuf, u"0101234789", 10));
    require(wmemmove_s_nid_postfix(memBuf, 10, memBuf + 2, 5) == 0);
    require(same(memBuf, u"0123434789", 10));
    require(wmemmove_s_nid_postfix(nullptr, 16, memBuf, 3) == 22);
    require(wmemmove_s_nid_postfix(memBuf, 16, nullptr, 3) == 22 && memBuf[0] == 0);
    require(wmemmove_s_nid_postfix(memBuf, 4, u"12345", 5) == 34 && memBuf[0] == 0);
    require(wmemmove_s_nid_postfix(memBuf, ((SIZE_MAX >> 1) / sizeof(char16_t)) + 1, u"a", 1) == 34);

    char16_t strBuf[16];
    require(wcsncpy_s_nid_postfix(strBuf, 16, u"hello", 5) == 0 && same(strBuf, u"hello", 6));
    require(wcsncpy_s_nid_postfix(strBuf, 16, u"hello world", 5) == 0 && same(strBuf, u"hello", 6));
    require(wcsncpy_s_nid_postfix(strBuf, 16, u"hi", 10) == 0 && same(strBuf, u"hi", 3));
    require(wcsncpy_s_nid_postfix(nullptr, 16, u"hi", 2) == 22);
    require(wcsncpy_s_nid_postfix(strBuf, 0, u"hi", 2) == 22);
    strBuf[0] = u'X';
    require(wcsncpy_s_nid_postfix(strBuf, 16, nullptr, 2) == 22 && strBuf[0] == 0);
    strBuf[0] = u'X';
    require(wcsncpy_s_nid_postfix(strBuf, 4, u"hello", 5) == 34 && strBuf[0] == 0);
    strBuf[0] = u'X';
    require(wcsncpy_s_nid_postfix(strBuf, 5, u"hello", 5) == 34 && strBuf[0] == 0);
    require(wcsncpy_s_nid_postfix(strBuf, ((SIZE_MAX >> 1) / sizeof(char16_t)) + 1, u"a", 1) == 34);
    wcsncpy_s_nid_postfix(strBuf, 16, u"abcdef", 6);

    require(wcsncpy_s_nid_postfix(strBuf + 1, 10, strBuf, 3) == 22 && strBuf[1] == 0);

    char16_t cpyBuf[16];
    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), 16, reinterpret_cast<const std::uint16_t*>(u"wide")) == 0 && same(cpyBuf, u"wide", 5));
    void* previousHandler = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(&ignore_handler_s_nid_postfix));
    require(wcscpy_s_nid_postfix(nullptr, 16, reinterpret_cast<const std::uint16_t*>(u"wide")) == 22);
    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), 0, reinterpret_cast<const std::uint16_t*>(u"wide")) == 22);
    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), 16, nullptr) == 22 && cpyBuf[0] == 0);
    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), 4, reinterpret_cast<const std::uint16_t*>(u"wide")) == 34 && cpyBuf[0] == 0);
    cpyBuf[0] = 0x1234;
    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), ((SIZE_MAX >> 1) / sizeof(char16_t)) + 1, reinterpret_cast<const std::uint16_t*>(u"a")) == 34 && cpyBuf[0] == 0x1234);
    wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf), 16, reinterpret_cast<const std::uint16_t*>(u"abcdef"));

    require(wcscpy_s_nid_postfix(reinterpret_cast<std::uint16_t*>(cpyBuf + 1), 10, reinterpret_cast<const std::uint16_t*>(cpyBuf)) == 22 && cpyBuf[1] == 0);
    set_constraint_handler_s_nid_postfix(previousHandler);

    char16_t ncatBuf[16];
    wmemset_nid_postfix(ncatBuf, 0xaaaa, 16);

    ncatBuf[0] = 0;
    require(wcsncat_nid_postfix(ncatBuf, u"hello", 3) == ncatBuf);
    require(same(ncatBuf, u"hel", 4) && ncatBuf[4] == 0xaaaa);
    require(wcsncat_nid_postfix(ncatBuf, u"lo world", 10) == ncatBuf);
    require(same(ncatBuf, u"hello world", 12) && ncatBuf[12] == 0xaaaa);

    wcsncpy_s_nid_postfix(strBuf, 16, u"hello", 5);
    require(wcsncat_s_nid_postfix(strBuf, 16, u" world", 6) == 0 && same(strBuf, u"hello world", 12));
    wcsncpy_s_nid_postfix(strBuf, 16, u"foo", 3);

    require(wcsncat_s_nid_postfix(strBuf, 16, u"barbaz", 3) == 0 && same(strBuf, u"foobar", 7));
    wcsncpy_s_nid_postfix(strBuf, 8, u"abc", 3);

    require(wcsncat_s_nid_postfix(strBuf, 8, u"1234", 4) == 0 && same(strBuf, u"abc1234", 8));
    wcsncpy_s_nid_postfix(strBuf, 8, u"abc", 3);

    require(wcsncat_s_nid_postfix(strBuf, 8, u"12345", 5) == 34 && strBuf[0] == 0);
    wmemset_nid_postfix(strBuf, u'x', 4);

    require(wcsncat_s_nid_postfix(strBuf, 4, u"a", 1) == 34 && strBuf[0] == 0);
    wcsncpy_s_nid_postfix(strBuf, 16, u"abc", 3);

    require(wcsncat_s_nid_postfix(strBuf, 16, strBuf + 1, 2) == 0 && same(strBuf, u"abcbc", 6));
    const char16_t splitSource[] = u"abc\0xyz";
    std::memcpy(strBuf, splitSource, sizeof(splitSource));
    require(wcsncat_s_nid_postfix(strBuf, 16, strBuf + 4, 3) == 22 && strBuf[0] == 0);
    wcsncpy_s_nid_postfix(strBuf, 16, u"abc", 3);

    require(wcscat_s_nid_postfix(strBuf, 16, u"def") == 0 && same(strBuf, u"abcdef", 7));
    wcsncpy_s_nid_postfix(strBuf, 8, u"12345", 5);

    require(wcscat_s_nid_postfix(strBuf, 8, u"6789") == 34 && strBuf[0] == 0);
    require(wcscat_s_nid_postfix(nullptr, 8, u"a") == 22);
    wcsncpy_s_nid_postfix(strBuf, 8, u"123", 3);

    require(wcscat_s_nid_postfix(strBuf, 8, nullptr) == 22 && strBuf[0] == 0);

    const char16_t* text = u"a世b世c";
    require(wcschr_nid_postfix(text, u'世') == text + 1);
    require(wcsrchr_nid_postfix(text, u'世') == text + 3);
    require(wcschr_nid_postfix(text, u'z') == nullptr);
    require(wcsrchr_nid_postfix(text, u'z') == nullptr);
    require(wcschr_nid_postfix(text, 0) == text + 5);
    require(wcsrchr_nid_postfix(text, 0) == text + 5);

    const char16_t* haystack = u"one two two";
    require(wcsstr_nid_postfix(haystack, u"two") == haystack + 4);
    require(wcsstr_nid_postfix(haystack, u"") == haystack);
    require(wcsstr_nid_postfix(haystack, u"three") == nullptr);
    require(wcsstr_nid_postfix(u"tw", u"two") == nullptr);

    require(wcspbrk_nid_postfix(haystack, u"wt") == haystack + 4);
    require(wcspbrk_nid_postfix(haystack, u"xyz") == nullptr);
    require(wcsspn_nid_postfix(u"aabbc", u"ab") == 4);
    require(wcsspn_nid_postfix(u"abc", u"") == 0);
    require(wcscspn_nid_postfix(u"abcde", u"dx") == 3 && wcscspn_nid_postfix(u"abc", u"") == 3 && wcscspn_nid_postfix(u"", u"a") == 0 && wcscspn_nid_postfix(u"abc", u"a") == 0);

    char16_t tokens[] = u",,one,,two;three,";
    char16_t* last = tokens;
    char16_t* token = wcstok_nid_postfix(tokens, u",", &last);
    require(token == tokens + 2 && same(token, u"one", 4) && last == tokens + 6);
    token = wcstok_nid_postfix(nullptr, u",;", &last);
    require(token == tokens + 7 && same(token, u"two", 4) && last == tokens + 11);
    token = wcstok_nid_postfix(nullptr, u",", &last);
    require(token == tokens + 11 && same(token, u"three", 6) && last == tokens + 17);
    require(wcstok_nid_postfix(nullptr, u",", &last) == nullptr && last == nullptr);
    require(wcstok_nid_postfix(nullptr, u",", &last) == nullptr && last == nullptr);

    char16_t tail[] = u"a b";
    require(wcstok_nid_postfix(tail, u" ", &last) == tail && last == tail + 2);
    require(wcstok_nid_postfix(nullptr, u" ", &last) == tail + 2 && same(tail, u"a\0b", 4) && last == nullptr);

    char16_t onlyDelimiters[] = u";;";
    last = onlyDelimiters;
    require(wcstok_nid_postfix(onlyDelimiters, u";", &last) == nullptr && last == nullptr && same(onlyDelimiters, u";;", 3));

    char16_t empty[] = u"";
    last = empty;
    require(wcstok_nid_postfix(empty, u";", &last) == nullptr && last == nullptr);

    char16_t whole[] = u"word";
    require(wcstok_nid_postfix(whole, u"", &last) == whole && last == nullptr && same(whole, u"word", 5));

    char16_t wide[] = u"\u4e00\u3001\xd83d\xde00\u3001";
    require(wcstok_nid_postfix(wide, u"\u3001", &last) == wide && same(wide, u"\u4e00\0", 2) && last == wide + 2);
    require(wcstok_nid_postfix(nullptr, u"\u3001", &last) == wide + 2 && wide[2] == 0xd83d && wide[3] == 0xde00 && wide[4] == 0 && last == wide + 5);
    require(wcstok_nid_postfix(nullptr, u"\u3001", &last) == nullptr && last == nullptr);

    const char16_t units[] = {u'x', 0, u'￿', u'y'};
    require(wmemchr_nid_postfix(units, u'￿', 4) == units + 2);
    require(wmemchr_nid_postfix(units, u'y', 3) == nullptr);
    const char16_t lower[] = {u'x', 0, u'\u0001', u'y'};
    require(wmemcmp_nid_postfix(units, units, 4) == 0);
    require(wmemcmp_nid_postfix(units, lower, 4) > 0);
    require(wmemcmp_nid_postfix(lower, units, 4) < 0);
    require(wmemcmp_nid_postfix(units, lower, 2) == 0);

    char16_t copy[4] = {};
    require(wmemcpy_nid_postfix(copy, units, 4) == copy && same(copy, units, 4));
    char16_t overlap[] = {u'1', u'2', u'3', u'4', u'5'};
    require(wmemmove_nid_postfix(overlap + 1, overlap, 4) == overlap + 1);
    const char16_t shifted[] = {u'1', u'1', u'2', u'3', u'4'};
    require(same(overlap, shifted, 5));
    require(wmemset_nid_postfix(copy, u'世', 3) == copy && copy[0] == u'世' && copy[2] == u'世' && copy[3] == u'y');

    require(wmemchr_nid_postfix(units, u'x', 0) == nullptr);
    require(wmemcmp_nid_postfix(units, lower, 0) == 0);
    char16_t untouched[] = {u'a', u'b', u'c', u'd', u'e'};
    require(wmemcpy_nid_postfix(untouched, units, 0) == untouched && same(untouched, u"abcde", 5));
    require(wmemmove_nid_postfix(untouched, units, 0) == untouched && same(untouched, u"abcde", 5));
    require(wmemset_nid_postfix(untouched, u'z', 0) == untouched && same(untouched, u"abcde", 5));
    require(wcsncpy_nid_postfix(untouched, u"xyz", 0) == untouched && same(untouched, u"abcde", 5));
    require(wmemmove_nid_postfix(untouched, untouched + 1, 4) == untouched && same(untouched, u"bcdee", 5));
    char16_t appended[] = {u'a', u'b', 0, u'z', u'z', 0};
    require(wcsncat_nid_postfix(appended, u"cd", 0) == appended && same(appended, u"ab\0zz", 6));
    require(wcsspn_nid_postfix(u"xyz", u"ab") == 0);
    require(wcspbrk_nid_postfix(haystack, u"") == nullptr);

    char16_t* end = nullptr;
    const char16_t* negative = u"  -42xyz";
    require(wcstol_nid_postfix(negative, &end, 10) == -42 && end == negative + 5);
    const char16_t* hex = u"0x1F!";
    require(wcstoll_nid_postfix(hex, &end, 16) == 31 && end == hex + 4);
    require(wcstoll_nid_postfix(hex, &end, 0) == 31 && end == hex + 4);
    const char16_t* wideSpace = u"　12";
    require(wcstoul_nid_postfix(wideSpace, &end, 10) == 0 && end == wideSpace);
    const char16_t* wideDigit = u"12١";
    require(wcstoll_nid_postfix(wideDigit, &end, 10) == 12 && end == wideDigit + 2);
    require(wcstoull_nid_postfix(u"18446744073709551615", nullptr, 10) == ULLONG_MAX);
    const char16_t* letters = u"abc";
    require(wcstol_nid_postfix(letters, &end, 10) == 0 && end == letters);
    const char16_t* binary = u" -0b101";
    for (const int base : {0, 2}) {
        require(wcstol_nid_postfix(binary, &end, base) == 0 && end == binary + 3);
        require(wcstoll_nid_postfix(binary, &end, base) == 0 && end == binary + 3);
        require(wcstoul_nid_postfix(binary, &end, base) == 0 && end == binary + 3);
        require(wcstoull_nid_postfix(binary, &end, base) == 0 && end == binary + 3);
    }
    require(wcstoll_nid_postfix(binary + 2, &end, 16) == 0xb101 && end == binary + 7);
    struct SignedCase {
        const char16_t* text;
        int base;
        long long value;
        std::ptrdiff_t consumed;
        int error;
    };
    for (const SignedCase& item : {SignedCase{u"- 1", 10, -1, 3, 0}, SignedCase{u"+\t-1", 10, LLONG_MAX, 4, 34}, SignedCase{u" - -1", 10, LLONG_MIN, 5, 34},
             SignedCase{u"-18446744073709551615", 10, LLONG_MIN, 21, 34}, SignedCase{u"-9223372036854775808", 10, LLONG_MIN, 20, 0},
             SignedCase{u"-9223372036854775809", 10, LLONG_MIN, 20, 34}, SignedCase{u"+-9223372036854775808", 10, LLONG_MAX, 21, 34}, SignedCase{u" -0x", 16, 0, 0, 0},
             SignedCase{u"- ", 10, 0, 0, 0}}) {
        for (auto* parse : {wcstol_nid_postfix, wcstoll_nid_postfix, wcstoimax_nid_postfix}) {
            *__error_nid_postfix() = 0;
            require(parse(item.text, &end, item.base) == item.value && end - item.text == item.consumed && *__error_nid_postfix() == item.error);
        }
    }

    const char16_t* scientific = u"3.5e2!";
    require(wcstod_nid_postfix(scientific, &end) == 350.0 && end == scientific + 5);
    require(wcstod_nid_postfix(letters, &end) == 0.0 && end == letters);
    require(wcstof_nid_postfix(u"0.25", nullptr) == 0.25f);
    const char16_t* sign = u" -x";
    require(std::signbit(wcstof_nid_postfix(sign, &end)) && end == sign && std::signbit(wcstold_nid_postfix(sign, &end)) && !std::signbit(wcstod_nid_postfix(sign, &end)));
    *__error_nid_postfix() = 0;
    require(wcstof_nid_postfix(u"0x1p+200", nullptr) == HUGE_VALF && *__error_nid_postfix() == ERANGE);
    *__error_nid_postfix() = 0;
    require(wcstof_nid_postfix(u"1e-45", nullptr) == 0x1p-149f && *__error_nid_postfix() == ERANGE);
    *__error_nid_postfix() = 0;
    require(wcstof_nid_postfix(u"0x1p-149", nullptr) == 0x1p-149f && *__error_nid_postfix() == 0);
    require(wcstof_nid_postfix(u"0x1.8p-150", nullptr) == 0x1p-149f && *__error_nid_postfix() == ERANGE);
    const char16_t* loneSign = u" -";
    const float loneSignValue = wcstof_nid_postfix(loneSign, &end);
    require(loneSignValue == 0.f && !std::signbit(loneSignValue) && end == loneSign && std::signbit(wcstold_nid_postfix(loneSign, &end)));
    const char16_t* half = u"-1.5é";
    require(wcstold_nid_postfix(half, &end) == -1.5L && end == half + 4);
    const char16_t* wideNan = u" -nan(0x123)é";
    require(std::bit_cast<std::uint64_t>(wcstod_nid_postfix(wideNan, &end)) == 0xfff8000000000123u && end == wideNan + 12);
    require(std::bit_cast<std::uint32_t>(wcstof_nid_postfix(wideNan, &end)) == 0xffc00123u && end == wideNan + 12);
    const long double wideExtended = wcstold_nid_postfix(wideNan, &end);
    std::uint64_t significand;
    std::memcpy(&significand, &wideExtended, sizeof(significand));
    require(std::isnan(wideExtended) && !std::signbit(wideExtended) && significand == 0xc000000000000000u && end == wideNan + 12);

    require(wcscoll_nid_postfix(u"a", u"b") < 0);
    require(wcscoll_nid_postfix(u"￿", u"a") > 0);
    require(wcscoll_nid_postfix(u"same", u"same") == 0);

    char16_t transformed[8];
    wmemset_nid_postfix(transformed, 0xaaaa, 8);

    require(wcsxfrm_nid_postfix(transformed, u"wide", 8) == 4 && same(transformed, u"wide", 5) && transformed[5] == 0xaaaa);
    wmemset_nid_postfix(transformed, 0xaaaa, 8);

    require(wcsxfrm_nid_postfix(transformed, u"much too long", 4) == 13);
    if (!same(transformed, u"much", 4)) std::fputs("wcsxfrm did not copy the first count units of its truncated result\n", stderr);
    require(same(transformed, u"much", 4) && transformed[4] == 0xaaaa);
    require(wcsxfrm_nid_postfix(nullptr, u"abc", 0) == 3);
    struct TransformCase {
        const char16_t* source;
        std::size_t count;
        std::size_t length;
        const char16_t* expected;
        std::size_t written;
    };
    for (const TransformCase test : {
        TransformCase{u"wide", 0, 4, u"", 0},
        TransformCase{u"wide", 1, 4, u"w", 1},
        TransformCase{u"wide", 4, 4, u"wide", 4},
        TransformCase{u"wide", 5, 4, u"wide", 5},
        TransformCase{u"", 0, 0, u"", 0},
        TransformCase{u"", 1, 0, u"", 1},
        TransformCase{u"", 8, 0, u"", 1},
        TransformCase{u"\x7fff\x8000\xffff\xd83d\xde00", 4, 5, u"\x7fff\x8000\xffff\xd83d", 4},
        TransformCase{u"\x7fff\x8000\xffff\xd83d\xde00", 6, 5, u"\x7fff\x8000\xffff\xd83d\xde00", 6}
    }) {
        wmemset_nid_postfix(transformed, 0xaaaa, 8);

        require(wcsxfrm_nid_postfix(transformed, test.source, test.count) == test.length);
        require(same(transformed, test.expected, test.written));
        for (std::size_t index = test.written; index < 8; ++index) require(transformed[index] == 0xaaaa);
    }
    {
        const char16_t number[] = u"  -0x7fffffffffffffff9";
        char16_t* wideEnd = nullptr;
        require(wcstoimax_nid_postfix(number, &wideEnd, 0) == INT64_MIN && wideEnd == number + 22);
        const char16_t positive[] = u"+42z";
        require(wcstoumax_nid_postfix(positive, &wideEnd, 10) == 42 && wideEnd == positive + 3);
        require(wcstoumax_nid_postfix(u"-1", nullptr, 10) == UINT64_MAX);
        char* narrowEnd = nullptr;
        const char text[] = "0x1ffffffffffffffffg";
        require(strtouq_nid_postfix(text, &narrowEnd, 16) == UINT64_MAX && narrowEnd == text + 19);
        const char signedText[] = "  -0x7fffffffffffffff9";
        require(strtoq_nid_postfix(signedText, &narrowEnd, 0) == INT64_MIN && narrowEnd == signedText + 22);
        const char positiveText[] = "+42z";
        require(strtoq_nid_postfix(positiveText, &narrowEnd, 10) == 42 && narrowEnd == positiveText + 3);
        const char path[] = "a/b/c";
        require(rindex_nid_postfix(path, '/') == path + 3 && rindex_nid_postfix(path, 'x') == nullptr);
        require(rindex_nid_postfix(path, 0) == path + 5);
        require(ffs_nid_postfix(0) == 0 && ffs_nid_postfix(1) == 1 && ffs_nid_postfix(0x80) == 8);
        require(ffs_nid_postfix(0x1000) == 13 && ffs_nid_postfix(INT_MIN) == 32 && ffs_nid_postfix(-1) == 1);
        require(ffsl_nid_postfix(0) == 0 && ffsl_nid_postfix(1) == 1 && ffsl_nid_postfix(0x80) == 8);
        require(ffsl_nid_postfix(0x100000000ll) == 33 && ffsl_nid_postfix(INT64_MIN) == 64 && ffsl_nid_postfix(-1) == 1);
        require(ffsll_nid_postfix(0) == 0 && ffsll_nid_postfix(1) == 1 && ffsll_nid_postfix(0x80) == 8);
        require(ffsll_nid_postfix(0x100000000ll) == 33 && ffsll_nid_postfix(LLONG_MIN) == 64 && ffsll_nid_postfix(-1) == 1);
        require(fls_nid_postfix(0) == 0 && fls_nid_postfix(1) == 1 && fls_nid_postfix(0x80) == 8);
        require(fls_nid_postfix(0x40000000) == 31 && fls_nid_postfix(INT_MIN) == 32 && fls_nid_postfix(-1) == 32);
        require(flsl_nid_postfix(0) == 0 && flsl_nid_postfix(1) == 1 && flsl_nid_postfix(0x80) == 8);
        require(flsl_nid_postfix(0x100000000ll) == 33 && flsl_nid_postfix(INT64_MIN) == 64 && flsl_nid_postfix(-1) == 64);
        require(flsll_nid_postfix(0) == 0 && flsll_nid_postfix(1) == 1 && flsll_nid_postfix(0x80) == 8);
        require(flsll_nid_postfix(0x100000000ll) == 33 && flsll_nid_postfix(LLONG_MIN) == 64 && flsll_nid_postfix(-1) == 64);
    }
    return 0;
}
