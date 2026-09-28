#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI strcoll_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strxfrm_nid_postfix(char*, const char*, std::size_t);
int APS5_VABI wcscoll_nid_postfix(const std::uint16_t*, const std::uint16_t*);
std::size_t APS5_VABI wcsxfrm_nid_postfix(std::uint16_t*, const std::uint16_t*, std::size_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(strcoll_nid_postfix("a", "b") < 0);
    Require(strcoll_nid_postfix("b", "a") > 0);
    Require(strcoll_nid_postfix("Ab", "Ab") == 0);
    Require(strcoll_nid_postfix("Z", "a") < 0);
    Require(strxfrm_nid_postfix(nullptr, "Doom", 0) == 4);
    std::array<char, 6> bytes{'?', '?', '?', '?', '?', '?'};
    Require(strxfrm_nid_postfix(bytes.data(), "Doom", 3) == 4);
    Require(bytes[0] == 'D' && bytes[1] == 'o' && bytes[2] == 'o' && bytes[3] == '?');
    Require(strxfrm_nid_postfix(bytes.data(), "Doom", 5) == 4);
    Require(std::strcmp(bytes.data(), "Doom") == 0 && bytes[5] == '?');
    Require(strxfrm_nid_postfix(bytes.data(), "", 1) == 0 && bytes[0] == '\0');

    const std::array<std::uint16_t, 3> a{u'A', 0xd83d, 0};
    const std::array<std::uint16_t, 3> b{u'A', 0xde00, 0};
    Require(wcscoll_nid_postfix(a.data(), b.data()) < 0);
    Require(wcscoll_nid_postfix(b.data(), a.data()) > 0);
    Require(wcscoll_nid_postfix(a.data(), a.data()) == 0);
    Require(wcsxfrm_nid_postfix(nullptr, a.data(), 0) == 2);
    std::array<std::uint16_t, 4> wide{0xbeef, 0xbeef, 0xbeef, 0xbeef};
    Require(wcsxfrm_nid_postfix(wide.data(), a.data(), 1) == 2);
    Require(wide[0] == u'A' && wide[1] == 0xbeef);
    Require(wcsxfrm_nid_postfix(wide.data(), a.data(), 3) == 2);
    Require(wide[0] == a[0] && wide[1] == a[1] && wide[2] == 0 && wide[3] == 0xbeef);
}
