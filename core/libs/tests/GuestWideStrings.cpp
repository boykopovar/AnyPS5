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
std::size_t APS5_VABI wcstombs_nid_postfix(char*, const std::uint16_t*, std::size_t);
std::size_t APS5_VABI mbrtowc_nid_postfix(std::uint16_t*, const char*, std::size_t, void*);
std::size_t APS5_VABI mbrlen_nid_postfix(const char*, std::size_t, void*);
int APS5_VABI mbtowc_nid_postfix(std::uint16_t*, const char*, std::size_t);
std::size_t APS5_VABI mbsrtowcs_nid_postfix(std::uint16_t*, const char**, std::size_t, void*);
std::size_t APS5_VABI wcrtomb_nid_postfix(char*, std::uint16_t, void*);
int* APS5_VABI __error_nid_postfix();
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
    const std::array<std::uint16_t, 4> ascii{u'A', u'B', u'C', 0};
    std::array<char, 5> bytes{'x', 'x', 'x', 'x', 'x'};
    Require(wcstombs_nid_postfix(nullptr, ascii.data(), 0) == 3);
    Require(wcstombs_nid_postfix(bytes.data(), ascii.data(), 0) == 0 && bytes[0] == 'x');
    Require(wcstombs_nid_postfix(bytes.data(), ascii.data(), 2) == 2);
    Require(bytes[0] == 'A' && bytes[1] == 'B' && bytes[2] == 'x');
    Require(wcstombs_nid_postfix(bytes.data(), ascii.data(), 3) == 3 && bytes[3] == 'x');
    Require(wcstombs_nid_postfix(bytes.data(), ascii.data(), 4) == 3 && bytes[3] == '\0');
    const std::array<std::uint16_t, 3> invalid{u'A', 0xd83d, 0};
    *__error_nid_postfix() = 0;
    Require(wcstombs_nid_postfix(bytes.data(), invalid.data(), 1) == 1);
    Require(*__error_nid_postfix() == 0);
    Require(wcstombs_nid_postfix(bytes.data(), invalid.data(), 2) == static_cast<std::size_t>(-1));
    Require(*__error_nid_postfix() == 86);
    Require(wcstombs_nid_postfix(nullptr, invalid.data(), 0) == static_cast<std::size_t>(-1));
    Require(wcstombs_nid_postfix(bytes.data(), nullptr, 4) == static_cast<std::size_t>(-1));
    Require(*__error_nid_postfix() == 22);

    // The guest's mbstate_t layout is opaque to this stateless C-locale decoder.
    std::array<unsigned char, 16> state{};
    std::uint16_t decoded = 0xbeef;
    *__error_nid_postfix() = 0;
    Require(mbrtowc_nid_postfix(&decoded, "A", 0, state.data()) == static_cast<std::size_t>(-2));
    Require(decoded == 0xbeef && *__error_nid_postfix() == 0);
    Require(mbrtowc_nid_postfix(&decoded, "A", 1, state.data()) == 1 && decoded == u'A');
    Require(mbrtowc_nid_postfix(&decoded, "", 1, state.data()) == 0 && decoded == 0);
    Require(mbrtowc_nid_postfix(nullptr, "Z", 1, nullptr) == 1);
    Require(mbrtowc_nid_postfix(&decoded, nullptr, 0, state.data()) == 0);
    Require(mbrtowc_nid_postfix(&decoded, "\x7f", 1, state.data()) == 1 && decoded == 0x7f);
    decoded = 0xbeef;
    Require(mbrtowc_nid_postfix(&decoded, "\x80", 1, state.data()) == static_cast<std::size_t>(-1));
    Require(decoded == 0xbeef && *__error_nid_postfix() == 86);
    Require(mbrlen_nid_postfix("A", 1, state.data()) == 1);
    Require(mbrlen_nid_postfix("", 1, state.data()) == 0);
    Require(mbrlen_nid_postfix("A", 0, state.data()) == static_cast<std::size_t>(-2));
    Require(mbtowc_nid_postfix(&decoded, nullptr, 0) == 0);
    Require(mbtowc_nid_postfix(&decoded, "B", 1) == 1 && decoded == u'B');
    Require(mbtowc_nid_postfix(&decoded, "B", 0) == -1 && *__error_nid_postfix() == 86);

    const char* source = "AB";
    std::array<std::uint16_t, 4> wide{0xbeef, 0xbeef, 0xbeef, 0xbeef};
    Require(mbsrtowcs_nid_postfix(wide.data(), &source, 1, state.data()) == 1);
    Require(wide[0] == u'A' && wide[1] == 0xbeef && *source == 'B');
    Require(mbsrtowcs_nid_postfix(wide.data() + 1, &source, 2, state.data()) == 1);
    Require(wide[1] == u'B' && wide[2] == 0 && source == nullptr);
    source = "ABC";
    Require(mbsrtowcs_nid_postfix(nullptr, &source, 0, state.data()) == 3);
    Require(source != nullptr && *source == 'A');
    source = "A\x80";
    Require(mbsrtowcs_nid_postfix(wide.data(), &source, 3, state.data()) == static_cast<std::size_t>(-1));
    Require(*source == static_cast<char>(0x80) && *__error_nid_postfix() == 86);
    source = "A";
    Require(mbsrtowcs_nid_postfix(wide.data(), &source, 0, state.data()) == 0 && *source == 'A');
    Require(mbsrtowcs_nid_postfix(wide.data(), nullptr, 2, state.data()) == static_cast<std::size_t>(-1));
    Require(*__error_nid_postfix() == 22);
    Require(wcrtomb_nid_postfix(nullptr, 0xffff, state.data()) == 1);
    char encoded = 'x';
    Require(wcrtomb_nid_postfix(&encoded, u'C', state.data()) == 1 && encoded == 'C');
    Require(wcrtomb_nid_postfix(&encoded, 0, state.data()) == 1 && encoded == '\0');
    Require(wcrtomb_nid_postfix(&encoded, 0xd83d, state.data()) == static_cast<std::size_t>(-1));
    Require(*__error_nid_postfix() == 86 && encoded == '\0');
    for (const auto byte : state) Require(byte == 0);
}
