#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ctype: %s\n", message);
    std::abort();
}

void TestNarrowCase() {
    GuestLocale::CtypeFacet facet{};
    facet.lower = _Getptolower_nid_postfix();
    facet.upper = _Getptoupper_nid_postfix();
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, 'A') == 'a', "tolower A");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, 'z') == 'z', "tolower z");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, '[') == '[', "tolower [");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, 'q') == 'Q', "toupper q");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, '7') == '7', "toupper 7");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, static_cast<char>(0xe4)) == static_cast<char>(0xe4), "toupper high byte");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, static_cast<char>(0xc4)) == static_cast<char>(0xc4), "tolower high byte");

    char text[] = "Mixed Case 42!";
    const char* end = text + std::strlen(text);
    Require(_ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(&facet, text, end) == end, "tolower range end");
    Require(std::strcmp(text, "mixed case 42!") == 0, "tolower range");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(&facet, text, end) == end, "toupper range end");
    Require(std::strcmp(text, "MIXED CASE 42!") == 0, "toupper range");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(&facet, text, text) == text, "empty range");
}

void TestWideCase() {
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, u'G') == u'g', "wide tolower G");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, u'-') == u'-', "wide tolower -");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, 0x4e2d) == 0x4e2d, "wide tolower cjk");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, 0xffff) == 0xffff, "wide tolower weof");

    std::array<std::uint16_t, 5> text {u'A', u'B', u'3', 0x4e2d, u'z'};
    const std::uint16_t* end = text.data() + text.size();
    Require(_ZNKSt5ctypeIwE10do_tolowerEPwPKw_nid_postfix(nullptr, text.data(), end) == end, "wide tolower range end");
    Require(text == std::array<std::uint16_t, 5> {u'a', u'b', u'3', 0x4e2d, u'z'}, "wide tolower range");
}

void TestWidenNarrow() {
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, 'x') == u'x', "widen x");
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, '\0') == 0, "widen nul");
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, static_cast<char>(0x80)) == 0xffff, "widen invalid byte");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, u'x', '?') == 'x', "narrow x");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, 0x00e9, '?') == '?', "narrow e acute");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, 0x4e2d, '*') == '*', "narrow cjk");

    const char bytes[] = {'a', 'Z', '\0', static_cast<char>(0x80), '7'};
    std::array<std::uint16_t, 5> wide {};
    Require(_ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, bytes, bytes + 5, wide.data()) == bytes + 5, "widen range end");
    Require(wide == std::array<std::uint16_t, 5> {u'a', u'Z', 0, 0xffff, u'7'}, "widen range");

    const std::array<std::uint16_t, 4> source {u'o', 0x00e9, u'k', 0x4e2d};
    std::array<char, 4> narrow {};
    Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data(), source.data() + source.size(), '#', narrow.data()) == source.data() + source.size(), "narrow range end");
    Require(narrow == std::array<char, 4> {'o', '#', 'k', '#'}, "narrow range");
    Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data(), source.data(), '#', narrow.data()) == source.data(), "narrow empty range");
}

}

int main() {
    TestNarrowCase();
    TestWideCase();
    TestWidenNarrow();
    return 0;
}
