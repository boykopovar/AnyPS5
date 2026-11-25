#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

extern "C" int __mb_cur_max_nid_postfix;

namespace {

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "multibyte locale helper check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

constexpr std::uint32_t WideEof = 0xffffffffu;

}

int main() {
    for (int c = -1; c <= 255; ++c) REQUIRE(_Btowc_nid_postfix(c) == btowc_nid_postfix(c));
    REQUIRE(_Btowc_nid_postfix('A') == u'A' && _Btowc_nid_postfix(0) == 0 && _Btowc_nid_postfix(-1) == WideEof);
    for (std::uint32_t w : {0u, 0x41u, 0x7fu, 0x80u, 0xffu, 0x100u, 0x263au, 0x10ffffu, WideEof}) REQUIRE(_Wctob_nid_postfix(w) == wctob_nid_postfix(w));
    REQUIRE(_Wctob_nid_postfix(u'A') == 'A' && _Wctob_nid_postfix(0) == 0 && _Wctob_nid_postfix(WideEof) == -1 && _Wctob_nid_postfix(0x263au) == -1);

    mbstate_t* const multibyte = _Getpmbstate_nid_postfix();
    mbstate_t* const wide = _Getpwcstate_nid_postfix();
    REQUIRE(multibyte != nullptr && wide != nullptr && multibyte != wide);
    REQUIRE(_Getpmbstate_nid_postfix() == multibyte && _Getpwcstate_nid_postfix() == wide);
    mbstate_t* otherMultibyte = nullptr;
    mbstate_t* otherWide = nullptr;
    std::thread other([&] {
        otherMultibyte = _Getpmbstate_nid_postfix();
        otherWide = _Getpwcstate_nid_postfix();
    });
    other.join();
    REQUIRE(otherMultibyte != nullptr && otherWide != nullptr && otherMultibyte != multibyte && otherWide != wide && otherMultibyte != otherWide);

    REQUIRE(___mb_cur_max_nid_postfix() == 1 && __mb_cur_max_nid_postfix == 1);
    std::puts("multibyte locale helpers passed");
    return 0;
}
