#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>

extern "C" {
const char16_t* APS5_VABI wmemchr_nid_postfix(const char16_t* s, char16_t c, std::size_t n);
int APS5_VABI wmemcmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::size_t n);
char16_t* APS5_VABI wmemcpy_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
char16_t* APS5_VABI wmemmove_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
std::size_t APS5_VABI wcslen_nid_postfix(const char16_t* s);
int APS5_VABI wcscmp_nid_postfix(const char16_t* s1, const char16_t* s2);
int APS5_VABI wcsncmp_nid_postfix(const char16_t* s1, const char16_t* s2, std::size_t n);
char16_t* APS5_VABI wcscpy_nid_postfix(char16_t* dest, const char16_t* src);
char16_t* APS5_VABI wcsncpy_nid_postfix(char16_t* dest, const char16_t* src, std::size_t n);
const char16_t* APS5_VABI wcschr_nid_postfix(const char16_t* s, char16_t c);
const char16_t* APS5_VABI wcsrchr_nid_postfix(const char16_t* s, char16_t c);
const char16_t* APS5_VABI wcsstr_nid_postfix(const char16_t* haystack, const char16_t* needle);
const char16_t* APS5_VABI wcspbrk_nid_postfix(const char16_t* s, const char16_t* accept);
std::size_t APS5_VABI wcsspn_nid_postfix(const char16_t* s, const char16_t* accept);
char16_t* APS5_VABI wmemset_nid_postfix(char16_t* s, char16_t c, std::size_t n);
double APS5_VABI wcstod_nid_postfix(const char16_t* str, char16_t** endptr);
float APS5_VABI wcstof_nid_postfix(const char16_t* str, char16_t** endptr);
long double APS5_VABI wcstold_nid_postfix(const char16_t* str, char16_t** endptr);
long long APS5_VABI wcstol_nid_postfix(const char16_t* str, char16_t** endptr, int base);
long long APS5_VABI wcstoll_nid_postfix(const char16_t* str, char16_t** endptr, int base);
unsigned long long APS5_VABI wcstoul_nid_postfix(const char16_t* str, char16_t** endptr, int base);
unsigned long long APS5_VABI wcstoull_nid_postfix(const char16_t* str, char16_t** endptr, int base);
unsigned long long APS5_VABI _WStoul_nid_postfix(const char16_t* str, char16_t** endptr, int base);
int APS5_VABI wcscoll_nid_postfix(const char16_t* first, const char16_t* second);
std::size_t APS5_VABI wcsxfrm_nid_postfix(char16_t* destination, const char16_t* source, std::size_t count);
}

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
    const char16_t split[] = {u'a', u'b', 0, u'c', 0};
    require(wcslen_nid_postfix(u"") == 0);
    require(wcslen_nid_postfix(u"héllo") == 5);
    require(wcslen_nid_postfix(split) == 2);

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

    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    require(wcsncpy_nid_postfix(buffer, u"ab", 5) == buffer);
    const char16_t padded[] = {u'a', u'b', 0, 0, 0, 0xaaaa};
    require(same(buffer, padded, 6));
    wmemset_nid_postfix(buffer, 0xaaaa, 8);
    wcsncpy_nid_postfix(buffer, u"abcdef", 3);
    const char16_t truncated[] = {u'a', u'b', u'c', 0xaaaa};
    require(same(buffer, truncated, 4));

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

    require(_WStoul_nid_postfix(negative, &end, 10) == ULLONG_MAX - 41 && end == negative + 5);
    require(_WStoul_nid_postfix(hex, &end, 16) == 31 && end == hex + 4);
    require(_WStoul_nid_postfix(hex, &end, 0) == 31 && end == hex + 4);
    require(_WStoul_nid_postfix(wideSpace, &end, 10) == 0 && end == wideSpace);
    require(_WStoul_nid_postfix(letters, &end, 10) == 0 && end == letters);
    require(_WStoul_nid_postfix(u"18446744073709551615", nullptr, 10) == ULLONG_MAX);
    for (const int base : {0, 2}) {
        require(_WStoul_nid_postfix(binary, &end, base) == 0 && end == binary + 3);
    }
    require(_WStoul_nid_postfix(binary + 2, &end, 16) == 0xb101 && end == binary + 7);

    const char16_t* scientific = u"3.5e2!";
    require(wcstod_nid_postfix(scientific, &end) == 350.0 && end == scientific + 5);
    require(wcstod_nid_postfix(letters, &end) == 0.0 && end == letters);
    require(wcstof_nid_postfix(u"0.25", nullptr) == 0.25f);
    const char16_t* half = u"-1.5é";
    require(wcstold_nid_postfix(half, &end) == -1.5L && end == half + 4);

    require(wcscoll_nid_postfix(u"a", u"b") < 0);
    require(wcscoll_nid_postfix(u"￿", u"a") > 0);
    require(wcscoll_nid_postfix(u"same", u"same") == 0);

    char16_t transformed[8];
    wmemset_nid_postfix(transformed, 0xaaaa, 8);
    require(wcsxfrm_nid_postfix(transformed, u"wide", 8) == 4 && same(transformed, u"wide", 5) && transformed[5] == 0xaaaa);
    require(wcsxfrm_nid_postfix(transformed, u"much too long", 4) == 13);
    require(wcsxfrm_nid_postfix(nullptr, u"abc", 0) == 3);

    const char16_t* aliasInputs[] = {negative, hex, wideSpace, wideDigit, letters, binary, scientific, half};
    const int aliasBases[] = {0, 2, 8, 10, 16, 36};
    for (const char16_t* input : aliasInputs) {
        for (const int base : aliasBases) {
            char16_t* aliasEnd = nullptr;
            char16_t* directEnd = nullptr;
            require(_WStoul_nid_postfix(input, &aliasEnd, base) == wcstoul_nid_postfix(input, &directEnd, base));
            require(aliasEnd == directEnd);
        }
    }
    return 0;
}
