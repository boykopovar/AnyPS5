#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace {

bool IsTrail(std::uint8_t value) {
    return value >= 0x80 && value <= 0xbf;
}

int TrailCount(std::int32_t lead) {
    if (lead < 0xc2 || lead > 0xf4) return 0;
    return lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
}

bool FirstTrail(std::int32_t lead, std::uint8_t trail, bool allowSurrogate) {
    if (!IsTrail(trail)) return false;
    if (lead == 0xe0) return trail >= 0xa0;
    if (lead == 0xed && !allowSurrogate) return trail < 0xa0;
    if (lead == 0xf0) return trail >= 0x90;
    if (lead == 0xf4) return trail < 0x90;
    return true;
}

bool Noncharacter(std::int32_t value) {
    return (value >= 0xfdd0 && value <= 0xfdef) || (value & 0xffff) >= 0xfffe;
}

std::int32_t ErrorValue(int trails, std::int8_t strict) {
    if (strict < 0) return strict == -3 ? 0xfffd : -1;
    constexpr std::array<std::int32_t, 4> values{0x15, 0x9f, 0xffff, 0x10ffff};
    return values[trails];
}

std::uint8_t Read(const std::uint8_t* text, std::int32_t index) {
    if (!text) APS5_INVALID_ARG_EX;
    return text[index];
}

void Advance(std::int32_t& index) {
    if (index == std::numeric_limits<std::int32_t>::max()) APS5_INVALID_ARG_EX;
    ++index;
}

struct Prefix {
    std::int32_t index;
    std::int32_t value = 0;
    int trails = 0;
    bool complete = false;
};

Prefix PreviousPrefix(const std::uint8_t* text, std::int32_t start, std::int32_t index, std::uint8_t last, bool allowSurrogate) {
    if (!IsTrail(last)) return {index};
    std::array<std::uint8_t, 3> trails{last};
    auto position = index;
    for (int count = 1; count <= 3 && position > start; ++count) {
        const auto byte = Read(text, --position);
        if (IsTrail(byte)) {
            if (count < 3) trails[count] = byte;
            continue;
        }
        const int needed = TrailCount(byte);
        if (needed < count || !FirstTrail(byte, trails[count - 1], allowSurrogate && count == 2 && needed == 2)) break;
        std::int32_t value = byte & (0x7f >> needed);
        for (int trail = count - 1; trail >= 0; --trail) value = (value << 6) | (trails[trail] & 0x3f);
        return {position, value, count, count == needed};
    }
    return {index};
}

void CheckBackward(std::int32_t start, std::int32_t index) {
    if (start < 0 || index < start) APS5_INVALID_ARG_EX;
}

void Write(std::uint8_t* text, std::int32_t index, std::uint32_t value, int width) {
    if (!text) APS5_INVALID_ARG_EX;
    for (int offset = width - 1; offset > 0; --offset) {
        text[index + offset] = static_cast<std::uint8_t>(0x80 | (value & 0x3f));
        value >>= 6;
    }
    constexpr std::array<std::uint8_t, 5> markers{0, 0, 0xc0, 0xe0, 0xf0};
    text[index] = static_cast<std::uint8_t>(markers[width] | value);
}

}

extern "C" {

std::int32_t APS5_VABI utf8_nextCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t* index, std::int32_t length, std::int32_t lead, std::int8_t strict) {
    if (!index || *index < 0 || (length >= 0 && *index > length)) APS5_INVALID_ARG_EX;
    const auto original = *index;
    auto position = original;
    const int needed = TrailCount(lead);
    if (!needed || position == length) return ErrorValue(0, strict);
    std::int32_t value = lead & (0x7f >> needed);
    for (int trail = 0; trail < needed && position != length; ++trail) {
        const auto byte = Read(text, position);
        if (trail == 0 ? !FirstTrail(lead, byte, strict == -2) : !IsTrail(byte)) break;
        value = (value << 6) | (byte & 0x3f);
        Advance(position);
    }
    *index = position;
    const int consumed = position - original;
    if (consumed == needed && (strict <= 0 || !Noncharacter(value))) return value;
    return ErrorValue(consumed, strict);
}

std::int32_t APS5_VABI utf8_prevCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t* index, std::int32_t trail, std::int8_t strict) {
    if (!index) APS5_INVALID_ARG_EX;
    CheckBackward(start, *index);
    const auto prefix = PreviousPrefix(text, start, *index, static_cast<std::uint8_t>(trail), strict == -2);
    *index = prefix.index;
    if (prefix.complete && (strict <= 0 || !Noncharacter(prefix.value))) return prefix.value;
    return ErrorValue(prefix.trails, strict);
}

std::int32_t APS5_VABI utf8_appendCharSafeBody_nid_postfix(std::uint8_t* text, std::int32_t index, std::int32_t capacity, std::int32_t value, std::int8_t* isError) {
    if (index < 0) APS5_INVALID_ARG_EX;
    const auto scalar = static_cast<std::uint32_t>(value);
    const int width = scalar <= 0x7ff ? 2 : scalar <= 0xffff ? 3 : 4;
    if (scalar <= 0x10ffff && index > std::numeric_limits<std::int32_t>::max() - (width - 1)) APS5_INVALID_ARG_EX;
    const std::int64_t available = static_cast<std::int64_t>(capacity) - index;
    if (scalar <= 0x10ffff && (scalar < 0xd800 || scalar > 0xdfff) && available >= width) {
        Write(text, index, scalar, width);
        return index + width;
    }
    if (isError) {
        *isError = 1;
    } else {
        if (available < std::numeric_limits<std::int32_t>::min()) APS5_INVALID_ARG_EX;
        if (available <= 0) return index;
        const auto count = static_cast<int>(std::min<std::int64_t>(available, 3));
        Write(text, index, static_cast<std::uint32_t>(ErrorValue(count - 1, 0)), count);
        return index + count;
    }
    return index;
}

std::int32_t APS5_VABI utf8_back1SafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t index) {
    CheckBackward(start, index);
    return PreviousPrefix(text, start, index, Read(text, index), false).index;
}

}
