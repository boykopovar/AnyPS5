#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {
constexpr int kGuestInet = 2;
constexpr int kGuestInet6 = 28;
constexpr std::size_t kMaxText = 46;

bool ParseDecimalPart(const char* begin, const char* end, std::uint8_t& value) {
    const std::ptrdiff_t length = end - begin;
    if (length < 1 || length > 3) return false;
    if (length > 1 && *begin == '0') return false;
    unsigned total = 0;
    for (const char* cursor = begin; cursor != end; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return false;
        total = total * 10 + static_cast<unsigned>(*cursor - '0');
    }
    if (total > 255) return false;
    value = static_cast<std::uint8_t>(total);
    return true;
}

bool ParseQuad(const char* begin, const char* end, std::uint8_t (&bytes)[4]) {
    const char* partBegin = begin;
    int parts = 0;
    for (const char* cursor = begin;; ++cursor) {
        if (cursor != end && *cursor != '.') continue;
        if (parts == 4 || !ParseDecimalPart(partBegin, cursor, bytes[parts])) return false;
        ++parts;
        if (cursor == end) break;
        partBegin = cursor + 1;
    }
    return parts == 4;
}

int HexValue(char symbol) {
    if (symbol >= '0' && symbol <= '9') return symbol - '0';
    if (symbol >= 'a' && symbol <= 'f') return symbol - 'a' + 10;
    if (symbol >= 'A' && symbol <= 'F') return symbol - 'A' + 10;
    return -1;
}

bool ParseHexGroup(const char* begin, const char* end, std::uint16_t& value) {
    const std::ptrdiff_t length = end - begin;
    if (length < 1 || length > 4) return false;
    unsigned total = 0;
    for (const char* cursor = begin; cursor != end; ++cursor) {
        const int digit = HexValue(*cursor);
        if (digit < 0) return false;
        total = total * 16 + static_cast<unsigned>(digit);
    }
    value = static_cast<std::uint16_t>(total);
    return true;
}

bool ParseGroupList(const char* begin, const char* end, bool mayEndWithQuad,
                    std::uint16_t (&groups)[8], int& count) {
    count = 0;
    if (begin == end) return true;
    const char* groupBegin = begin;
    for (const char* cursor = begin;; ++cursor) {
        if (cursor != end && *cursor != ':') continue;
        const bool last = cursor == end;
        bool hasDot = false;
        for (const char* probe = groupBegin; probe != cursor; ++probe) hasDot = hasDot || *probe == '.';
        if (hasDot) {
            std::uint8_t quad[4];
            if (!last || !mayEndWithQuad || count > 6 || !ParseQuad(groupBegin, cursor, quad)) return false;
            groups[count++] = static_cast<std::uint16_t>(quad[0] << 8 | quad[1]);
            groups[count++] = static_cast<std::uint16_t>(quad[2] << 8 | quad[3]);
            return true;
        }
        if (count == 8 || !ParseHexGroup(groupBegin, cursor, groups[count])) return false;
        ++count;
        if (last) return true;
        groupBegin = cursor + 1;
    }
}

bool TextToV6(const char* text, std::uint8_t (&bytes)[16]) {
    const char* const end = text + std::strlen(text);
    const char* gap = std::strstr(text, "::");
    std::uint16_t head[8];
    std::uint16_t tail[8];
    int headCount = 0;
    int tailCount = 0;
    if (!gap) {
        if (!ParseGroupList(text, end, true, head, headCount) || headCount != 8) return false;
    } else {
        if (std::strstr(gap + 1, "::")) return false;
        if (!ParseGroupList(text, gap, false, head, headCount)) return false;
        if (!ParseGroupList(gap + 2, end, true, tail, tailCount)) return false;
        if (headCount + tailCount > 7) return false;
    }
    std::uint16_t words[8]{};
    for (int index = 0; index < headCount; ++index) words[index] = head[index];
    for (int index = 0; index < tailCount; ++index) words[8 - tailCount + index] = tail[index];
    for (int index = 0; index < 8; ++index) {
        bytes[index * 2] = static_cast<std::uint8_t>(words[index] >> 8);
        bytes[index * 2 + 1] = static_cast<std::uint8_t>(words[index]);
    }
    return true;
}

std::size_t AppendDecimal(char* out, unsigned value) {
    char digits[3];
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value);
    for (std::size_t index = 0; index < count; ++index) out[index] = digits[count - 1 - index];
    return count;
}

std::size_t FormatQuad(const std::uint8_t* bytes, char* out) {
    std::size_t length = 0;
    for (int index = 0; index < 4; ++index) {
        if (index) out[length++] = '.';
        length += AppendDecimal(out + length, bytes[index]);
    }
    return length;
}

std::size_t AppendHex(char* out, unsigned value) {
    static const char symbols[] = "0123456789abcdef";
    char digits[4];
    std::size_t count = 0;
    do {
        digits[count++] = symbols[value & 15];
        value >>= 4;
    } while (value);
    for (std::size_t index = 0; index < count; ++index) out[index] = digits[count - 1 - index];
    return count;
}

void FindLongestZeroRun(const std::uint16_t (&words)[8], int& start, int& length) {
    start = -1;
    length = 0;
    for (int from = 0; from < 8; ++from) {
        if (words[from] != 0) continue;
        int run = 0;
        while (from + run < 8 && words[from + run] == 0) ++run;
        if (run > length) {
            start = from;
            length = run;
        }
        from += run;
    }
    if (length < 2) {
        start = -1;
        length = 0;
    }
}

std::size_t V6ToText(const std::uint8_t* bytes, char* out) {
    std::uint16_t words[8];
    for (int index = 0; index < 8; ++index)
        words[index] = static_cast<std::uint16_t>(bytes[index * 2] << 8 | bytes[index * 2 + 1]);
    int runStart;
    int runLength;
    FindLongestZeroRun(words, runStart, runLength);
    std::size_t length = 0;
    if (runStart == 0 && (runLength == 6 || (runLength == 5 && words[5] == 0xffff))) {
        const char* prefix = runLength == 6 ? "::" : "::ffff:";
        length = std::strlen(prefix);
        std::memcpy(out, prefix, length);
        return length + FormatQuad(bytes + 12, out + length);
    }
    for (int index = 0; index < 8;) {
        if (index == runStart) {
            out[length++] = ':';
            if (index + runLength == 8) out[length++] = ':';
            index += runLength;
            continue;
        }
        if (index) out[length++] = ':';
        length += AppendHex(out + length, words[index]);
        ++index;
    }
    return length;
}
}

extern "C" {
int APS5_VABI __inet_pton_nid_postfix(int family, const char* text, void* destination) {
    if (family != kGuestInet && family != kGuestInet6) { *__error_nid_postfix() = 47; return -1; }
    if (!text || !destination) { *__error_nid_postfix() = 14; return -1; }
    if (family == kGuestInet) {
        std::uint8_t quad[4];
        if (!ParseQuad(text, text + std::strlen(text), quad)) return 0;
        std::memcpy(destination, quad, sizeof(quad));
        return 1;
    }
    std::uint8_t address[16];
    if (!TextToV6(text, address)) return 0;
    std::memcpy(destination, address, sizeof(address));
    return 1;
}

const char* APS5_VABI __inet_ntop_nid_postfix(int family, const void* source,
                                            char* destination, std::uint32_t capacity) {
    if (family != kGuestInet && family != kGuestInet6) { *__error_nid_postfix() = 47; return nullptr; }
    if (!source || !destination) { *__error_nid_postfix() = 14; return nullptr; }
    char text[kMaxText + 1];
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    const std::size_t length = family == kGuestInet ? FormatQuad(bytes, text) : V6ToText(bytes, text);
    text[length] = '\0';
    if (length + 1 > capacity) { *__error_nid_postfix() = 28; return nullptr; }
    std::memcpy(destination, text, length + 1);
    return destination;
}
}
