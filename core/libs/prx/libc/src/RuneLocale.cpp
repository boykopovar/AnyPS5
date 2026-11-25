#include "LibcxxRegexCollation.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::optional<char> FindCollatingElement(std::string_view name) {
    if (name.size() == 1) {
        const char character = name.front();
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) return character;
    }
    using LibcxxRegex::CollatingElements;
    const auto* entry = std::ranges::lower_bound(CollatingElements, name, {}, &LibcxxRegex::CollatingElement::name);
    if (entry != std::end(CollatingElements) && entry->name == name) return entry->character;
    return std::nullopt;
}

struct ShortString {
    unsigned char sizeAndTag;
    char characters[23];
};
static_assert(sizeof(ShortString) == 24);

}

namespace GuestRune {

struct RangeEntry {
    std::int32_t min;
    std::int32_t max;
    std::int32_t map;
    const std::uint64_t* types;
};
static_assert(offsetof(RangeEntry, types) == 16);
static_assert(sizeof(RangeEntry) == 24);

struct Range {
    std::int32_t count;
    const RangeEntry* entries;
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

const Locale* ActiveLocale() {
    return _CurrentRuneLocale_nid_postfix != nullptr ? _CurrentRuneLocale_nid_postfix : &_DefaultRuneLocale_nid_postfix;
}

const RangeEntry* FindEntry(const Range& range, std::int32_t rune) {
    if (rune < 0 || range.count <= 0 || range.entries == nullptr) return nullptr;
    std::int32_t low = 0;
    std::int32_t high = range.count - 1;
    while (low <= high) {
        const std::int32_t mid = low + (high - low) / 2;
        const RangeEntry& entry = range.entries[mid];
        if (rune < entry.min) {
            high = mid - 1;
        } else if (rune > entry.max) {
            low = mid + 1;
        } else {
            return &entry;
        }
    }
    return nullptr;
}

int Is(std::uint32_t c, std::uint64_t mask) {
    const Locale* locale = ActiveLocale();
    if (c < 256) return (locale->types[c] & mask) != 0;
    if (c > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return 0;
    const auto rune = static_cast<std::int32_t>(c);
    const RangeEntry* entry = FindEntry(locale->typeRanges, rune);
    if (entry == nullptr) return 0;
    const std::uint64_t type = entry->types != nullptr
        ? entry->types[rune - entry->min]
        : static_cast<std::uint64_t>(static_cast<std::uint32_t>(entry->map));
    return (type & mask) != 0;
}

std::uint32_t MapCase(std::uint32_t c, const std::int32_t (Locale::*table)[256], const Range Locale::*ranges) {
    const Locale* locale = ActiveLocale();
    if (c < 256) return static_cast<std::uint32_t>((locale->*table)[c]);
    if (c > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return c;
    const auto rune = static_cast<std::int32_t>(c);
    const RangeEntry* entry = FindEntry(locale->*ranges, rune);
    if (entry == nullptr) return c;
    return static_cast<std::uint32_t>(entry->map) + static_cast<std::uint32_t>(rune - entry->min);
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
int APS5_VABI iswascii_nid_postfix(std::uint32_t c) { return (c & ~0x7Fu) == 0; }
int APS5_VABI iswctype_nid_postfix(std::uint32_t c, std::uint64_t mask) { return GuestRune::Is(c, mask); }

int APS5_VABI _Iswctype_nid_postfix(std::uint32_t c, std::uint64_t type) {
    static constexpr std::uint64_t masks[] = {
        GuestRune::Alpha | GuestRune::Number, GuestRune::Alpha, GuestRune::Control, GuestRune::Digit,
        GuestRune::Graph, GuestRune::Lower, GuestRune::Print, GuestRune::Punct,
        GuestRune::Space, GuestRune::Upper, GuestRune::Hex, GuestRune::Blank,
    };
    if (type == 0 || type == std::size(masks) + 1) return 0;
    if (type > std::size(masks)) throw std::invalid_argument("_Iswctype: unknown class " + std::to_string(type));
    return GuestRune::Is(c, masks[type - 1]);
}

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
    std::string name;
    if (CopyGuestString_nid_no_patch(property, 10, name) != 0) return 0;
    for (const auto& entry : properties) {
        if (name == entry.name) return entry.mask;
    }
    return 0;
}

std::uint64_t APS5_VABI _ZNSt3__115__get_classnameEPKcb_nid_postfix(const char* property, bool caseInsensitive) {
    constexpr auto alnum = GuestRune::Alpha | GuestRune::Digit;
    constexpr auto letterCase = GuestRune::Lower | GuestRune::Upper;
    constexpr auto word = 0x80 | alnum | letterCase;
    static constexpr struct {
        const char* name;
        std::uint64_t mask;
    } classes[] = {
        {"alnum", alnum}, {"alpha", GuestRune::Alpha}, {"blank", GuestRune::Blank},
        {"cntrl", GuestRune::Control}, {"d", GuestRune::Digit}, {"digit", GuestRune::Digit},
        {"graph", alnum | GuestRune::Punct}, {"lower", GuestRune::Lower}, {"print", GuestRune::Print},
        {"punct", GuestRune::Punct}, {"s", GuestRune::Space}, {"space", GuestRune::Space},
        {"upper", GuestRune::Upper}, {"w", word}, {"xdigit", GuestRune::Hex},
    };
    std::string name;
    if (CopyGuestString_nid_no_patch(property, sizeof("xdigit"), name) != 0) return 0;
    for (const auto& entry : classes) {
        if (name != entry.name) continue;
        if (caseInsensitive && (entry.mask & letterCase) != 0) return entry.mask | GuestRune::Alpha;
        return entry.mask;
    }
    return 0;
}

void* APS5_VABI _ZNSt3__120__get_collation_nameEPKc_nid_postfix(void* result, const char* name) {
    ShortString value{};
    std::string element;
    if (CopyGuestString_nid_no_patch(name, sizeof("right-square-bracket"), element) == 0) {
        if (const auto character = FindCollatingElement(element)) {
            value.sizeAndTag = 2;
            value.characters[0] = *character;
        }
    }
    if (!GuestCopyOut_nid_no_patch(result, &value, sizeof(value))) {
        throw std::invalid_argument("libc++ collation name: unwritable result");
    }
    return result;
}

std::uint32_t APS5_VABI towlower_nid_postfix(std::uint32_t c) {
    return GuestRune::MapCase(c, &GuestRune::Locale::lower, &GuestRune::Locale::lowerRanges);
}

std::uint32_t APS5_VABI towupper_nid_postfix(std::uint32_t c) {
    return GuestRune::MapCase(c, &GuestRune::Locale::upper, &GuestRune::Locale::upperRanges);
}

std::uint32_t APS5_VABI btowc_nid_postfix(int c) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == -1) return GuestRune::Weof;
    if (byte < 0x80) return byte;
    if (byte < 0xc0 || byte >= 0xf0) errno = 86;
    return GuestRune::Weof;
}

int APS5_VABI wctob_nid_postfix(std::uint32_t c) {
    const auto unit = static_cast<std::uint16_t>(c);
    return unit < 0x80 ? static_cast<int>(unit) : -1;
}

std::uint32_t APS5_VABI _Btowc_nid_postfix(int c) {
    return btowc_nid_postfix(c);
}

int APS5_VABI _Wctob_nid_postfix(std::uint32_t c) {
    return wctob_nid_postfix(c);
}

std::uint64_t APS5_VABI wctrans_nid_postfix(const char* property) {
    std::string name;
    if (CopyGuestString_nid_no_patch(property, 8, name) != 0) return 0;
    if (name == "tolower") return 1;
    if (name == "toupper") return 2;
    return 0;
}

std::uint32_t APS5_VABI towctrans_nid_postfix(std::uint32_t c, std::uint64_t desc) {
    return desc == 1 || desc == 2 ? _Towctrans_nid_postfix(c, desc) : c;
}

std::uint32_t APS5_VABI _Towlower_nid_postfix(std::uint32_t c, const void*) {
    return _Towctrans_nid_postfix(c, 1);
}

std::uint32_t APS5_VABI _Towupper_nid_postfix(std::uint32_t c, const void*) {
    return _Towctrans_nid_postfix(c, 2);
}

int APS5_VABI _Tolower_nid_postfix(int c, const void*) {
    return static_cast<unsigned>(c) <= 0x7f ? _Getptolower_nid_postfix()[c] : c;
}

int APS5_VABI _Toupper_nid_postfix(int c, const void*) {
    return static_cast<unsigned>(c) <= 0x7f ? _Getptoupper_nid_postfix()[c] : c;
}

}
