#pragma once

#include <cstddef>
#include <cstdint>

#include "ImeFilterCodepages.hpp"

namespace ime_filter {

struct FilterParam {
    std::uint64_t mask;
    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t variant;
};

static_assert(sizeof(FilterParam) == 24);

constexpr std::uint32_t kErrorInvalidParam = 0x80bc0030u;
constexpr std::uint32_t kErrorInvalidAddress = 0x80bc0031u;

constexpr std::uint64_t kClassCyrillic = 0x200000400ull;
constexpr std::uint64_t kClassVietnamese = 0x40000000ull;
constexpr std::uint64_t kClassRomanian = 0x80000000ull;
constexpr std::uint64_t kClassThai = 0x4000000ull;

constexpr std::uint64_t kAbsentMasks[8] = {0x188000000ull, 0x40000000ull, 0x10000000ull, 0x1000000ull,
                                           0x2000ull,       0x18000ull,   0x6000ull,     0x4000ull};

constexpr char16_t kPunctuationLatin[] = {
    0xa1,   0xbf,   0xb4,   0x2018, 0x2019, 0x201a, 0x201c, 0x201d, 0x201e, 0xb0,   0x2039, 0x203a,
    0xab,   0xbb,   0xaa,   0xba,   0xd7,   0xf7,   0xa4,   0xa2,   0x20ac, 0xa3,   0xa5,   0x20a9,
    0xa7,   0xa6,   0xb5,   0xac,   0xb7,   0xb9,   0xb2,   0xb3,   0xbc,   0xbd,   0xbe,   0x2116};

constexpr char16_t kPunctuationFullwidth[] = {
    0x3001, 0x3002, 0x30fc, 0x30fb, 0x300c, 0x300d, 0xffe5, 0xff01, 0x201d, 0xff03, 0xff04, 0xff05, 0xff06,
    0x2019, 0xff08, 0xff09, 0xff0a, 0xff0b, 0xff0c, 0xff0d, 0xff0e, 0xff0f, 0xff1a, 0xff1b, 0xff1c, 0xff1d,
    0xff1e, 0xff1f, 0xff20, 0xff3b, 0xff3c, 0xff3d, 0xff3e, 0xff3f, 0xff40, 0xff5b, 0xff5c, 0xff5d, 0xff5e};

constexpr char16_t kPunctuationFullwidthNoProlong[] = {
    0x3001, 0x3002, 0x300c, 0x300d, 0xff01, 0x201d, 0xff03, 0xff04, 0xff05, 0xff06, 0x2019, 0xff08,
    0xff09, 0xff0a, 0xff0b, 0xff0c, 0xff0d, 0xff0e, 0xff0f, 0xff1a, 0xff1b, 0xff1c, 0xff1d, 0xff1e,
    0xff1f, 0xff20, 0xff3b, 0xff3c, 0xff3d, 0xff3e, 0xff3f, 0xff40, 0xff5b, 0xff5c, 0xff5d, 0xff5e};

constexpr char16_t kVietnameseLetters[] = {0x1a0, 0x1a1, 0x1af, 0x1b0, 0x20ab};
constexpr char16_t kModifierLetters[] = {0x2c7, 0x2d8, 0x2d9, 0x2db, 0x2dd};
constexpr char16_t kNumberSeparators[] = {0x2c, 0x2e, 0x2d, 0x0a};

template <std::size_t N>
constexpr bool In(const char16_t (&set)[N], std::uint32_t c) {
    for (char16_t v : set) {
        if (v == c) return true;
    }
    return false;
}

constexpr bool InCodepageA(std::uint32_t c) {
    return ((kCodepageABitmap[c >> 6] >> (c & 63)) & 1u) != 0;
}

constexpr bool InCodepageB(std::uint32_t c) {
    constexpr std::size_t count = sizeof(kCodepageBRanges) / sizeof(kCodepageBRanges[0]);
    std::size_t low = 0;
    std::size_t high = count;
    while (low < high) {
        const std::size_t middle = (low + high) / 2;
        if (c > kCodepageBRanges[middle].last) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low < count && c >= kCodepageBRanges[low].first;
}

constexpr bool IsVietnameseExtension(std::uint32_t c) {
    return static_cast<std::uint16_t>(c - 0x300) < 0x24 || (c & 0xff00) == 0x1e00 || In(kVietnameseLetters, c);
}

constexpr std::uint64_t DerivedMask(std::uint32_t version, std::uint32_t variant) {
    const std::uint32_t second = variant == 1 ? 0x10000000u : 0x6000000u;
    const std::uint64_t flag = (variant == 0 || version >= 0x2000000u) ? (1u << 24) : 0;
    std::uint64_t low = flag + 0x1fffffu;
    const std::uint64_t high = flag | 0x21fffffu;
    if (version >= 0x2500000u || variant == 0) low = high;
    const std::uint64_t wide = 0x200000000ull + low - 0x4000000ull;
    if (version >= 0x5000000u || variant == 0) low = wide;
    return version < second ? low : (low | 0x200000000ull);
}

constexpr bool ValidMask(std::uint64_t mask, std::uint32_t version, std::uint32_t variant, std::uint32_t* limit) {
    const bool none = variant == 0;
    const bool wide = none || version >= 0x5000000u;
    const std::uint64_t shifted = (none || version >= 0x2500000u ? 1u : 0u) << 25;
    const std::uint64_t first = wide ? 0x200000000ull + shifted - 0x17e1e401ull : (shifted | 0x1e1bffull);
    if ((mask & first) != 0) {
        *limit = 0x17f;
        return true;
    }
    *limit = 0x7f;
    const std::uint32_t second = variant == 1 ? 0x10000000u : 0x6000000u;
    const std::uint64_t flag = static_cast<std::uint64_t>(none || version >= 0x2000000u ? 1u : 0u) << 24;
    const std::uint64_t base = (wide ? 0x1401e400ull : 0x1e400ull) | flag;
    const std::uint64_t allowed = version < second ? base : (base | 0x200000000ull);
    return (mask & allowed) != 0;
}

constexpr bool HighBitSet(std::uint64_t mask) {
    return (mask & 0x80000000ull) != 0;
}

constexpr bool KeepClassified(std::uint32_t c, std::uint64_t mask, std::uint32_t limit, const bool (&absent)[8]) {
    if (c < limit) return true;
    if (In(kPunctuationLatin, c)) return true;
    if (!absent[7] && (static_cast<std::uint16_t>(c - 0x3130) < 0x60 || static_cast<std::uint16_t>(c + 0x5400) < 0x2bb0)) {
        return true;
    }
    if ((kClassCyrillic & mask) != 0 && (c & 0xff00) == 0x400) return true;
    if (!absent[6] && (static_cast<std::uint16_t>(c + 0xf0) <= 9 || In(kPunctuationFullwidth, c))) return true;
    if (!absent[5] && (static_cast<std::uint16_t>(c + 0xf0) <= 9 || In(kPunctuationFullwidthNoProlong, c))) return true;
    if (!absent[4] && InCodepageA(c)) return true;
    if (!absent[5] && InCodepageB(c)) return true;
    if (!absent[3] && ((c & 0xff00) == 0x600 || (c & 0xfffc) == 0x200c)) return true;
    if ((mask & kClassThai) != 0 && (c & 0xff80) == 0xe00) return true;
    if (!absent[2] && (static_cast<std::uint16_t>(c - 0x370) < 0x72 || (c & 0xfff0) == 0x3f0)) return true;
    if (!absent[1] && IsVietnameseExtension(c)) return true;
    if (HighBitSet(mask) && (c & 0xfffc) == 0x218) return true;
    if (!absent[0] && In(kModifierLetters, c)) return true;
    return false;
}

inline std::uint32_t FilterText(char16_t* dst, std::int32_t* out_length, std::uint32_t capacity,
                                const FilterParam* param, const char16_t* src, std::uint32_t src_length) {
    if (dst == nullptr || out_length == nullptr || param == nullptr || src == nullptr) return kErrorInvalidAddress;
    if (capacity < 2) return kErrorInvalidParam;
    const std::uint32_t type = param->type;
    if (type >= 5 && type - 0x100 > 3) return kErrorInvalidParam;

    std::uint64_t mask = param->mask;
    if (mask == 0) mask = DerivedMask(param->version, param->variant);
    std::uint32_t limit = 0;
    if (!ValidMask(mask, param->version, param->variant, &limit)) return kErrorInvalidParam;

    bool absent[8];
    for (int i = 0; i < 8; ++i) absent[i] = (mask & kAbsentMasks[i]) == 0;

    std::uint32_t room = capacity - 1;
    std::uint32_t count = 0;
    for (std::uint64_t i = 0; i < src_length && room != 0; ++i) {
        const std::uint32_t c = src[i];
        if (c == 0) break;
        bool keep;
        if (type == 4) {
            keep = static_cast<std::uint16_t>(c - 0x30) <= 9 || In(kNumberSeparators, c);
        } else if (type == 1 || type == 3 || type == 0x102) {
            keep = c <= 0x7e;
        } else if (type == 0x100) {
            keep = c < 0x17f;
            if (!keep && (mask & kClassVietnamese) != 0) {
                const bool listed = static_cast<std::uint16_t>(c - 0x300) >= 0x24 && (c & 0xff00) != 0x1e00;
                keep = !listed || In(kVietnameseLetters, c);
            }
            if (!keep && HighBitSet(mask)) keep = (c & 0xfffc) == 0x218;
        } else {
            keep = KeepClassified(c, mask, limit, absent);
        }
        if (keep) {
            dst[count++] = static_cast<char16_t>(c);
            --room;
        }
    }
    dst[count] = 0;
    *out_length = static_cast<std::int32_t>(count);
    return 0;
}

}
