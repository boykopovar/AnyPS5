#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace GuestRune {

struct Range {
    std::int32_t count;
    void* entries;
};

struct Locale {
    char magic[8];
    char encoding[32];
    void* getRune;
    void* putRune;
    std::int32_t invalidRune;
    std::uint64_t types[256];
    std::int32_t lower[256];
    std::int32_t upper[256];
    Range typeRanges;
    Range lowerRanges;
    Range upperRanges;
    void* variable;
    std::int32_t variableLength;
};
static_assert(offsetof(Locale, invalidRune) == 56);
static_assert(offsetof(Locale, types) == 64);
static_assert(offsetof(Locale, lower) == 2112);
static_assert(offsetof(Locale, upper) == 3136);
static_assert(offsetof(Locale, typeRanges) == 4160);
static_assert(offsetof(Locale, variable) == 4208);
static_assert(sizeof(Locale) == 4224);

constexpr std::uint64_t Alpha = 0x100;
constexpr std::uint64_t Control = 0x200;
constexpr std::uint64_t Digit = 0x400;
constexpr std::uint64_t Graph = 0x800;
constexpr std::uint64_t Lower = 0x1000;
constexpr std::uint64_t Punct = 0x2000;
constexpr std::uint64_t Space = 0x4000;
constexpr std::uint64_t Upper = 0x8000;
constexpr std::uint64_t Hex = 0x10000;
constexpr std::uint64_t Blank = 0x20000;
constexpr std::uint64_t Print = 0x40000;
constexpr std::uint64_t Number = 0x400000;

constexpr std::uint64_t Type(int c) {
    if (c == '\t') return Control | Space | Blank;
    if (c >= '\n' && c <= '\r') return Control | Space;
    if (c < ' ' || c == 0x7f) return Control;
    if (c == ' ') return Space | Blank | Print;
    if (c >= '0' && c <= '9') return Digit | Print | Graph | Hex | Number | static_cast<std::uint64_t>(c - '0');
    if (c >= 'A' && c <= 'F') return Upper | Hex | Print | Graph | Alpha | static_cast<std::uint64_t>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return Lower | Hex | Print | Graph | Alpha | static_cast<std::uint64_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'Z') return Upper | Print | Graph | Alpha;
    if (c >= 'a' && c <= 'z') return Lower | Print | Graph | Alpha;
    if (c < 0x7f) return Punct | Print | Graph;
    return 0;
}

constexpr Locale MakeDefault() {
    Locale locale{{'R', 'u', 'n', 'e', 'M', 'a', 'g', 'i'}, "NONE", nullptr, nullptr, 0xfffd, {}, {}, {}, {}, {}, {}, nullptr, 0};
    for (int c = 0; c < 256; ++c) {
        locale.types[c] = Type(c);
        locale.lower[c] = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        locale.upper[c] = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
    }
    return locale;
}

}

extern "C" {

extern const GuestRune::Locale _DefaultRuneLocale_nid_postfix;
const GuestRune::Locale _DefaultRuneLocale_nid_postfix = GuestRune::MakeDefault();
const GuestRune::Locale* _CurrentRuneLocale_nid_postfix = &_DefaultRuneLocale_nid_postfix;
int __mb_sb_limit_nid_postfix = 256;

}

namespace GuestRune {

constexpr std::uint32_t Weof = 0xffffffff;

int Is(std::uint32_t c, std::uint64_t mask) {
    return c < 256 && (_CurrentRuneLocale_nid_postfix->types[c] & mask) != 0;
}

}

extern "C" {

int APS5_VABI iswalnum_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Alpha | GuestRune::Number); }
int APS5_VABI iswalpha_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Alpha); }
int APS5_VABI iswblank_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Blank); }
int APS5_VABI iswcntrl_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Control); }
int APS5_VABI iswdigit_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Digit); }
int APS5_VABI iswgraph_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Graph); }
int APS5_VABI iswlower_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Lower); }
int APS5_VABI iswprint_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Print); }
int APS5_VABI iswpunct_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Punct); }
int APS5_VABI iswspace_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Space); }
int APS5_VABI iswupper_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Upper); }
int APS5_VABI iswxdigit_nid_postfix(std::uint32_t c) { return GuestRune::Is(c, GuestRune::Hex); }
int APS5_VABI iswctype_nid_postfix(std::uint32_t c, std::uint64_t mask) { return GuestRune::Is(c, mask); }

std::uint64_t APS5_VABI wctype_nid_postfix(const char* property) {
    static constexpr struct {
        const char* name;
        std::uint64_t mask;
    } properties[] = {
        {"alnum", GuestRune::Alpha | GuestRune::Number}, {"alpha", GuestRune::Alpha}, {"blank", GuestRune::Blank},
        {"cntrl", GuestRune::Control}, {"digit", GuestRune::Digit}, {"graph", GuestRune::Graph},
        {"lower", GuestRune::Lower}, {"print", GuestRune::Print}, {"punct", GuestRune::Punct},
        {"space", GuestRune::Space}, {"upper", GuestRune::Upper}, {"xdigit", GuestRune::Hex},
        {"ideogram", 0x80000}, {"special", 0x100000}, {"phonogram", 0x200000},
        {"number", GuestRune::Number}, {"rune", 0xffffff00},
    };
    for (const auto& entry : properties) {
        if (std::strcmp(property, entry.name) == 0) return entry.mask;
    }
    return 0;
}

std::uint32_t APS5_VABI towlower_nid_postfix(std::uint32_t c) {
    return c < 256 ? static_cast<std::uint32_t>(_CurrentRuneLocale_nid_postfix->lower[c]) : c;
}

std::uint32_t APS5_VABI towupper_nid_postfix(std::uint32_t c) {
    return c < 256 ? static_cast<std::uint32_t>(_CurrentRuneLocale_nid_postfix->upper[c]) : c;
}

std::uint32_t APS5_VABI btowc_nid_postfix(int c) {
    return c == -1 ? GuestRune::Weof : static_cast<unsigned char>(c);
}

int APS5_VABI wctob_nid_postfix(std::uint32_t c) {
    return c < 256 ? static_cast<int>(c) : -1;
}

}
