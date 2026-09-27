#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
std::size_t APS5_VABI wcslen_nid_postfix(const std::uint16_t*);
int APS5_VABI wcscmp_nid_postfix(const std::uint16_t*, const std::uint16_t*);
int APS5_VABI wcsncmp_nid_postfix(const std::uint16_t*, const std::uint16_t*, std::size_t);
std::uint16_t* APS5_VABI wcscpy_nid_postfix(std::uint16_t*, const std::uint16_t*);
std::uint16_t* APS5_VABI wcsncpy_nid_postfix(std::uint16_t*, const std::uint16_t*, std::size_t);
std::uint16_t* APS5_VABI wcscat_nid_postfix(std::uint16_t*, const std::uint16_t*);
std::uint16_t* APS5_VABI wcsncat_nid_postfix(std::uint16_t*, const std::uint16_t*, std::size_t);
const std::uint16_t* APS5_VABI wcschr_nid_postfix(const std::uint16_t*, std::uint16_t);
const std::uint16_t* APS5_VABI wcsrchr_nid_postfix(const std::uint16_t*, std::uint16_t);
const std::uint16_t* APS5_VABI wcsstr_nid_postfix(const std::uint16_t*, const std::uint16_t*);
const std::uint16_t* APS5_VABI wmemchr_nid_postfix(const std::uint16_t*, std::uint16_t, std::size_t);
int APS5_VABI wmemcmp_nid_postfix(const std::uint16_t*, const std::uint16_t*, std::size_t);
std::uint16_t* APS5_VABI wmemcpy_nid_postfix(std::uint16_t*, const std::uint16_t*, std::size_t);
std::uint16_t* APS5_VABI wmemmove_nid_postfix(std::uint16_t*, const std::uint16_t*, std::size_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const std::array<std::uint16_t, 5> text{u'A', 0xd83d, 0xde00, 0xff10, 0};
    const std::array<std::uint16_t, 3> emoji{0xd83d, 0xde00, 0};
    Require(wcslen_nid_postfix(text.data()) == 4);
    Require(wcslen_nid_postfix(text.data() + 4) == 0);
    Require(wcscmp_nid_postfix(text.data(), text.data()) == 0);
    Require(wcscmp_nid_postfix(text.data(), emoji.data()) < 0);
    Require(wcsncmp_nid_postfix(text.data(), emoji.data(), 0) == 0);
    Require(wcsncmp_nid_postfix(text.data() + 1, emoji.data(), 2) == 0);
    Require(wcschr_nid_postfix(text.data(), 0xde00) == text.data() + 2);
    Require(wcsrchr_nid_postfix(text.data(), 0) == text.data() + 4);
    Require(wcsstr_nid_postfix(text.data(), emoji.data()) == text.data() + 1);
    Require(wcsstr_nid_postfix(text.data(), text.data() + 4) == text.data());
    Require(wmemchr_nid_postfix(text.data(), 0xde00, 2) == nullptr);
    Require(wmemchr_nid_postfix(text.data(), 0xde00, 3) == text.data() + 2);
    Require(wmemcmp_nid_postfix(text.data(), emoji.data(), 1) < 0);
    std::array<std::uint16_t, 10> copied{};
    Require(wcscpy_nid_postfix(copied.data(), text.data()) == copied.data());
    Require(wcscmp_nid_postfix(copied.data(), text.data()) == 0);
    Require(wcscat_nid_postfix(copied.data(), emoji.data()) == copied.data());
    Require(wcslen_nid_postfix(copied.data()) == 6);
    Require(wcsncat_nid_postfix(copied.data(), text.data(), 1) == copied.data());
    Require(copied[6] == u'A' && copied[7] == 0);
    copied.fill(0xbeef);
    Require(wcsncpy_nid_postfix(copied.data(), emoji.data(), 4) == copied.data());
    Require(copied[0] == 0xd83d && copied[1] == 0xde00 && copied[2] == 0 && copied[3] == 0 && copied[4] == 0xbeef);
    Require(wmemcpy_nid_postfix(copied.data(), text.data(), 5) == copied.data());
    Require(wmemmove_nid_postfix(copied.data() + 1, copied.data(), 5) == copied.data() + 1);
    Require(wmemcmp_nid_postfix(copied.data() + 1, text.data(), 5) == 0);
}
