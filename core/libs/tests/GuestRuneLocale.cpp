#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

extern "C" {
extern const unsigned char _DefaultRuneLocale_nid_postfix[4224];
extern const unsigned char* _CurrentRuneLocale_nid_postfix;
extern int __mb_sb_limit_nid_postfix;
int APS5_VABI isalpha_nid_postfix(int);
int APS5_VABI iscntrl_nid_postfix(int);
int APS5_VABI isdigit_nid_postfix(int);
int APS5_VABI isgraph_nid_postfix(int);
int APS5_VABI islower_nid_postfix(int);
int APS5_VABI isprint_nid_postfix(int);
int APS5_VABI ispunct_nid_postfix(int);
int APS5_VABI isspace_nid_postfix(int);
int APS5_VABI isupper_nid_postfix(int);
int APS5_VABI isxdigit_nid_postfix(int);
int APS5_VABI isblank_nid_postfix(int);
int APS5_VABI tolower_nid_postfix(int);
int APS5_VABI toupper_nid_postfix(int);
int APS5_VABI iswalnum_nid_postfix(std::uint32_t);
int APS5_VABI iswalpha_nid_postfix(std::uint32_t);
int APS5_VABI iswblank_nid_postfix(std::uint32_t);
int APS5_VABI iswcntrl_nid_postfix(std::uint32_t);
int APS5_VABI iswdigit_nid_postfix(std::uint32_t);
int APS5_VABI iswgraph_nid_postfix(std::uint32_t);
int APS5_VABI iswlower_nid_postfix(std::uint32_t);
int APS5_VABI iswprint_nid_postfix(std::uint32_t);
int APS5_VABI iswpunct_nid_postfix(std::uint32_t);
int APS5_VABI iswspace_nid_postfix(std::uint32_t);
int APS5_VABI iswupper_nid_postfix(std::uint32_t);
int APS5_VABI iswxdigit_nid_postfix(std::uint32_t);
int APS5_VABI iswctype_nid_postfix(std::uint32_t, std::uint64_t);
std::uint64_t APS5_VABI wctype_nid_postfix(const char*);
std::uint32_t APS5_VABI towlower_nid_postfix(std::uint32_t);
std::uint32_t APS5_VABI towupper_nid_postfix(std::uint32_t);
std::uint64_t APS5_VABI wctrans_nid_postfix(const char*);
std::uint32_t APS5_VABI towctrans_nid_postfix(std::uint32_t, std::uint64_t);
std::uint32_t APS5_VABI _Towlower_nid_postfix(std::uint32_t, const void*);
std::uint32_t APS5_VABI _Towupper_nid_postfix(std::uint32_t, const void*);
int APS5_VABI _Tolower_nid_postfix(int, const void*);
int APS5_VABI _Toupper_nid_postfix(int, const void*);
std::uint32_t APS5_VABI btowc_nid_postfix(int);
int APS5_VABI wctob_nid_postfix(std::uint32_t);
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Rune locale check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename T>
T Field(const unsigned char* locale, std::size_t offset) {
    T value;
    std::memcpy(&value, locale + offset, sizeof(value));
    return value;
}

int main() {
    const unsigned char* locale = _DefaultRuneLocale_nid_postfix;
    Require(_CurrentRuneLocale_nid_postfix == locale);
    Require(__mb_sb_limit_nid_postfix == 256);
    Require(std::memcmp(locale, "RuneMagi", 8) == 0);
    Require(std::strcmp(reinterpret_cast<const char*>(locale + 8), "NONE") == 0);
    Require(Field<void*>(locale, 40) == nullptr && Field<void*>(locale, 48) == nullptr);
    Require(Field<std::int32_t>(locale, 56) == 0xfffd);
    for (std::size_t offset = 4160; offset < 4224; ++offset) Require(locale[offset] == 0);
    const struct {
        std::uint64_t bit;
        int (APS5_VABI* classify)(int);
    } classes[] = {
        {0x100, isalpha_nid_postfix}, {0x200, iscntrl_nid_postfix}, {0x400, isdigit_nid_postfix},
        {0x800, isgraph_nid_postfix}, {0x1000, islower_nid_postfix}, {0x2000, ispunct_nid_postfix},
        {0x4000, isspace_nid_postfix}, {0x8000, isupper_nid_postfix}, {0x10000, isxdigit_nid_postfix},
        {0x20000, isblank_nid_postfix}, {0x40000, isprint_nid_postfix},
    };
    for (int c = 0; c < 256; ++c) {
        const auto type = Field<std::uint64_t>(locale, 64 + 8 * c);
        for (const auto& entry : classes) Require(((type & entry.bit) != 0) == (entry.classify(c) != 0));
        Require(((type & 0x400000) != 0) == (isdigit_nid_postfix(c) != 0));
        Require((type & ~std::uint64_t{0x47ffff}) == 0);
        if (isxdigit_nid_postfix(c)) {
            const int value = c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
            Require(static_cast<int>(type & 0xff) == value);
        } else {
            Require((type & 0xff) == 0);
        }
        Require(Field<std::int32_t>(locale, 2112 + 4 * c) == tolower_nid_postfix(c));
        Require(Field<std::int32_t>(locale, 3136 + 4 * c) == toupper_nid_postfix(c));
    }
    Require(Field<std::uint64_t>(locale, 64 + 8 * 0x80) == 0 && Field<std::uint64_t>(locale, 64 + 8 * 0xff) == 0);

    const struct {
        const char* name;
        std::uint64_t mask;
        int (APS5_VABI* wide)(std::uint32_t);
        int (APS5_VABI* narrow)(int);
    } wide[] = {
        {"alnum", 0x400100, iswalnum_nid_postfix, nullptr}, {"alpha", 0x100, iswalpha_nid_postfix, isalpha_nid_postfix},
        {"blank", 0x20000, iswblank_nid_postfix, isblank_nid_postfix}, {"cntrl", 0x200, iswcntrl_nid_postfix, iscntrl_nid_postfix},
        {"digit", 0x400, iswdigit_nid_postfix, isdigit_nid_postfix}, {"graph", 0x800, iswgraph_nid_postfix, isgraph_nid_postfix},
        {"lower", 0x1000, iswlower_nid_postfix, islower_nid_postfix}, {"print", 0x40000, iswprint_nid_postfix, isprint_nid_postfix},
        {"punct", 0x2000, iswpunct_nid_postfix, ispunct_nid_postfix}, {"space", 0x4000, iswspace_nid_postfix, isspace_nid_postfix},
        {"upper", 0x8000, iswupper_nid_postfix, isupper_nid_postfix}, {"xdigit", 0x10000, iswxdigit_nid_postfix, isxdigit_nid_postfix},
    };
    for (const auto& entry : wide) {
        Require(wctype_nid_postfix(entry.name) == entry.mask);
        for (std::uint32_t c = 0; c < 300; ++c) {
            const int expected = c < 256 && (entry.narrow ? entry.narrow(static_cast<int>(c)) : isalpha_nid_postfix(static_cast<int>(c)) || isdigit_nid_postfix(static_cast<int>(c)));
            Require((entry.wide(c) != 0) == (expected != 0));
            Require((iswctype_nid_postfix(c, entry.mask) != 0) == (expected != 0));
        }
        Require(entry.wide(0xffffffff) == 0 && iswctype_nid_postfix(0xffffffff, entry.mask) == 0);
    }
    Require(wctype_nid_postfix("ideogram") == 0x80000 && wctype_nid_postfix("special") == 0x100000);
    Require(wctype_nid_postfix("phonogram") == 0x200000 && wctype_nid_postfix("number") == 0x400000);
    Require(wctype_nid_postfix("rune") == 0xffffff00 && wctype_nid_postfix("") == 0 && wctype_nid_postfix("Alpha") == 0);
    Require(iswctype_nid_postfix('A', 0) == 0 && iswctype_nid_postfix('7', wctype_nid_postfix("number")) != 0);
    for (std::uint32_t c = 0; c < 300; ++c) {
        Require(towlower_nid_postfix(c) == (c < 256 ? static_cast<std::uint32_t>(tolower_nid_postfix(static_cast<int>(c))) : c));
        Require(towupper_nid_postfix(c) == (c < 256 ? static_cast<std::uint32_t>(toupper_nid_postfix(static_cast<int>(c))) : c));
    }
    Require(towlower_nid_postfix(0xffffffff) == 0xffffffff && towupper_nid_postfix(0xffffffff) == 0xffffffff);
    Require(btowc_nid_postfix(-1) == 0xffffffff && btowc_nid_postfix(0) == 0);
    Require(btowc_nid_postfix('A') == 'A' && btowc_nid_postfix(0x7f) == 0x7f && btowc_nid_postfix(0x80) == 0xffffffff);
    Require(btowc_nid_postfix(0xe9) == 0xffffffff && btowc_nid_postfix(-23) == 0xffffffff && btowc_nid_postfix(0x141) == 'A');
    Require(wctob_nid_postfix('A') == 'A' && wctob_nid_postfix(0x7f) == 0x7f && wctob_nid_postfix(0x80) == -1 && wctob_nid_postfix(0) == 0);
    Require(wctob_nid_postfix(0xe9) == -1 && wctob_nid_postfix(0x20ac) == -1 && wctob_nid_postfix(0xffffffff) == -1);
    Require(wctob_nid_postfix(0x10041) == 'A' && wctob_nid_postfix(0x80000000) == 0);
    Require(wctrans_nid_postfix("tolower") == 1 && wctrans_nid_postfix("toupper") == 2);
    Require(wctrans_nid_postfix("") == 0 && wctrans_nid_postfix("TOLOWER") == 0 && wctrans_nid_postfix("tolowe") == 0);
    for (std::int64_t c = -300; c < 70000; ++c) {
        const auto wide = static_cast<std::uint32_t>(c);
        const bool ascii = c >= 0 && c < 128;
        const auto lower = ascii ? static_cast<std::uint32_t>(std::tolower(static_cast<int>(c))) : wide;
        const auto upper = ascii ? static_cast<std::uint32_t>(std::toupper(static_cast<int>(c))) : wide;
        Require(towctrans_nid_postfix(wide, 1) == lower && towctrans_nid_postfix(wide, 2) == upper);
        Require(_Towlower_nid_postfix(wide, nullptr) == lower && _Towupper_nid_postfix(wide, nullptr) == upper);
        Require(_Tolower_nid_postfix(static_cast<int>(c), nullptr) == static_cast<int>(lower));
        Require(_Toupper_nid_postfix(static_cast<int>(c), nullptr) == static_cast<int>(upper));
    }
    errno = 0;
    Require(towctrans_nid_postfix('A', 0) == 'A' && towctrans_nid_postfix('a', 3) == 'a' && errno == 0);
    for (int c = -300; c < 0x1200; ++c) {
        const auto byte = static_cast<unsigned char>(c);
        errno = 0;
        const std::uint32_t wide = btowc_nid_postfix(c);
        const bool invalidLead = c != -1 && ((byte >= 0x80 && byte < 0xc0) || byte >= 0xf0);
        Require(wide == (c != -1 && byte < 0x80 ? byte : 0xffffffffu) && errno == (invalidLead ? 86 : 0));
    }
    errno = 7;
    Require(btowc_nid_postfix(0xc3) == 0xffffffff && btowc_nid_postfix(0xef) == 0xffffffff && btowc_nid_postfix(-1) == 0xffffffff && errno == 7);
    Require(btowc_nid_postfix(0x41) == 'A' && errno == 7);
    return 0;
}
