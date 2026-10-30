#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

bool IsLead(std::int32_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool IsTrail(std::int32_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

std::int32_t Supplementary(std::int32_t lead, std::int32_t trail) {
    return 0x10000 + ((lead - 0xd800) << 10) + trail - 0xdc00;
}

int Digit(char16_t value, int base) {
    int digit = value >= u'0' && value <= u'9' ? value - u'0'
        : value >= u'a' && value <= u'f' ? value - u'a' + 10
        : value >= u'A' && value <= u'F' ? value - u'A' + 10 : -1;
    return digit < base ? digit : -1;
}

std::int32_t Decode(Icu::UnescapeCharacterAt characterAt, std::int32_t& position, std::int32_t length, void* context) {
    const auto start = position;
    if (position < 0 || position >= length) return -1;
    if (!characterAt) throw std::invalid_argument("u_unescapeAt: null callback");
    const auto initial = characterAt(position++, context);
    bool braces = false;
    int minimum = 0;
    int maximum = 0;
    int digits = 0;
    int base = 16;
    std::uint32_t value = 0;
    switch (initial) {
    case u'u': minimum = maximum = 4; break;
    case u'U': minimum = maximum = 8; break;
    case u'x':
        minimum = 1;
        braces = position < length && characterAt(position, context) == u'{';
        maximum = braces ? 8 : 2;
        if (braces) ++position;
        break;
    default:
        if (initial >= u'0' && initial <= u'7') {
            minimum = 1;
            maximum = 3;
            digits = 1;
            base = 8;
            value = initial - u'0';
        }
        break;
    }
    if (minimum) {
        char16_t last = initial;
        while (position < length && digits < maximum) {
            last = characterAt(position, context);
            const int digit = Digit(last, base);
            if (digit < 0) break;
            value = value * base + digit;
            ++position;
            ++digits;
        }
        if (digits < minimum || (braces && last != u'}') || value > 0x10ffff) {
            position = start;
            return -1;
        }
        if (braces) ++position;
        if (IsLead(value) && position < length) {
            auto ahead = position + 1;
            std::int32_t tail = characterAt(position, context);
            if (tail == u'\\' && ahead < length) {
                const auto limit = ahead + std::min<std::int32_t>(11, length - ahead);
                tail = Decode(characterAt, ahead, limit, context);
            }
            if (IsTrail(tail)) {
                position = ahead;
                return Supplementary(value, tail);
            }
        }
        return static_cast<std::int32_t>(value);
    }
    switch (initial) {
    case u'a': return 7;
    case u'b': return 8;
    case u'e': return 27;
    case u'f': return 12;
    case u'n': return 10;
    case u'r': return 13;
    case u't': return 9;
    case u'v': return 11;
    default: break;
    }
    const bool control = initial == u'c' && position < length;
    std::int32_t result = control ? characterAt(position++, context) : initial;
    if (IsLead(result) && position < length) {
        const auto tail = characterAt(position, context);
        if (IsTrail(tail)) {
            ++position;
            result = Supplementary(result, tail);
        }
    }
    return control ? result & 0x1f : result;
}

char16_t APS5_VABI ByteAt(std::int32_t position, void* context) {
    return static_cast<const unsigned char*>(context)[position];
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_unescapeAt_nid_postfix(Icu::UnescapeCharacterAt characterAt, std::int32_t* offset, std::int32_t length, void* context) {
    if (!offset) APS5_INVALID_ARG_EX;
    return Decode(characterAt, *offset, length, context);
}

std::int32_t APS5_VABI u_unescape_nid_postfix(const char* source, char16_t* destination, std::int32_t capacity) {
    if (!source) APS5_INVALID_ARG_EX;
    const auto bytes = std::strlen(source);
    if (bytes > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        NotImplemented_nid_no_patch(__func__);
    const auto length = static_cast<std::int32_t>(bytes);
    const auto room = [&](std::int32_t used) {
        if (destination && static_cast<std::int64_t>(capacity) - used < std::numeric_limits<std::int32_t>::min())
            NotImplemented_nid_no_patch("u_unescape: capacity minus the output length overflows");
    };
    std::int32_t written = 0;
    std::int32_t segment = 0;
    for (std::int32_t position = 0; position < length;) {
        const auto byte = static_cast<unsigned char>(source[position++]);
        if (byte != '\\') {
            if (destination && written < capacity) destination[written] = byte;
            ++written;
            continue;
        }
        if (written != segment) room(segment);
        const auto value = Decode(ByteAt, position, length, const_cast<char*>(source));
        if (value < 0) {
            if (destination && capacity > 0) destination[0] = 0;
            return 0;
        }
        room(written);
        const int units = value > 0xffff ? 2 : 1;
        if (destination && written <= capacity && units <= capacity - written) {
            if (units == 1) destination[written] = static_cast<char16_t>(value);
            else {
                destination[written] = static_cast<char16_t>(0xd800 + ((value - 0x10000) >> 10));
                destination[written + 1] = static_cast<char16_t>(0xdc00 + ((value - 0x10000) & 0x3ff));
            }
        }
        written += units;
        segment = written;
    }
    if (written != segment) room(segment);
    if (destination && written < capacity) destination[written] = 0;
    return written;
}

}
