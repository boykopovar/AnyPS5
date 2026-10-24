#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "SceTypes.hpp"

namespace {

constexpr int NP_ERROR_INVALID_FORMAT = static_cast<int>(0x80558003);
constexpr int NP_ERROR_INVALID_CHARACTER = static_cast<int>(0x80558004);
constexpr int NP_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80558008);
constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80558009);
constexpr int NP_ERROR_OUT_OF_RANGE = static_cast<int>(0x8055800E);

constexpr std::uint32_t NP_FORMAT_UPPERCASE = 1;
constexpr std::uint32_t NP_FORMAT_PREFIX = 2;
constexpr std::uint32_t NP_FORMAT_SIGN = 4;
constexpr std::uint32_t NP_RADIX_MAX = 36;

constexpr char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char BASE64URL_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr char BASE64_PAD = '=';

int Base64Value(char c, const char* alphabet) {
    if (c == '\0') return -1;
    const char* found = std::strchr(alphabet, c);
    return found != nullptr ? static_cast<int>(found - alphabet) : -1;
}

struct Base64Block {
    std::uint32_t bits;
    int bytes;
};

int DecodeLastBlock(const char* src, std::size_t length, const char* alphabet, Base64Block* block) {
    char c[4] = {BASE64_PAD, BASE64_PAD, BASE64_PAD, BASE64_PAD};
    std::memcpy(c, src, length);
    const int v0 = Base64Value(c[0], alphabet);
    const int v1 = Base64Value(c[1], alphabet);
    if (v0 < 0 || v1 < 0) return NP_ERROR_INVALID_CHARACTER;
    block->bits = (static_cast<std::uint32_t>(v0) << 18) | (static_cast<std::uint32_t>(v1) << 12);
    if (c[2] == BASE64_PAD) {
        if (c[3] != BASE64_PAD) return NP_ERROR_INVALID_CHARACTER;
        block->bytes = 1;
        return 0;
    }
    const int v2 = Base64Value(c[2], alphabet);
    if (v2 < 0) return NP_ERROR_INVALID_CHARACTER;
    block->bits |= static_cast<std::uint32_t>(v2) << 6;
    if (c[3] == BASE64_PAD) {
        block->bytes = 2;
        return 0;
    }
    const int v3 = Base64Value(c[3], alphabet);
    if (v3 < 0) return NP_ERROR_INVALID_CHARACTER;
    block->bits |= static_cast<std::uint32_t>(v3);
    block->bytes = 3;
    return 0;
}

int Base64Decode(const char* src, std::size_t length, std::uint8_t* dst, std::size_t capacity, const char* alphabet,
    bool padded) {
    if (padded && (length & 3) != 0) return NP_ERROR_INVALID_FORMAT;
    int decoded = 0;
    while (length > 4) {
        std::uint32_t bits = 0;
        for (int i = 0; i < 4; ++i) {
            const int value = Base64Value(src[i], alphabet);
            if (value < 0) return NP_ERROR_INVALID_CHARACTER;
            bits = (bits << 6) | static_cast<std::uint32_t>(value);
        }
        if (dst != nullptr) {
            if (capacity < 3) return NP_ERROR_INVALID_FORMAT;
            capacity -= 3;
            *dst++ = static_cast<std::uint8_t>(bits >> 16);
            *dst++ = static_cast<std::uint8_t>(bits >> 8);
            *dst++ = static_cast<std::uint8_t>(bits);
        }
        decoded += 3;
        src += 4;
        length -= 4;
    }
    if (padded && length != 4) return NP_ERROR_INVALID_FORMAT;
    Base64Block block {};
    const int result = DecodeLastBlock(src, length, alphabet, &block);
    if (result < 0) return result;
    if (dst != nullptr) {
        if (capacity < static_cast<std::size_t>(block.bytes)) return NP_ERROR_INVALID_FORMAT;
        for (int i = 0; i < block.bytes; ++i) dst[i] = static_cast<std::uint8_t>(block.bits >> (16 - 8 * i));
    }
    return decoded + block.bytes;
}

int Base64Encode(const std::uint8_t* src, char* dst, std::size_t length, const char* alphabet, bool padded) {
    char* out = dst;
    for (; length >= 3; length -= 3, src += 3) {
        const std::uint32_t bits = (src[0] << 16) | (src[1] << 8) | src[2];
        for (int shift = 18; shift >= 0; shift -= 6) *out++ = alphabet[(bits >> shift) & 0x3F];
    }
    if (length != 0) {
        const std::uint32_t bits = (src[0] << 16) | (length == 2 ? src[1] << 8 : 0);
        *out++ = alphabet[(bits >> 18) & 0x3F];
        *out++ = alphabet[(bits >> 12) & 0x3F];
        if (length == 2) *out++ = alphabet[(bits >> 6) & 0x3F];
        if (padded) {
            if (length == 1) *out++ = BASE64_PAD;
            *out++ = BASE64_PAD;
        }
    }
    return static_cast<int>(out - dst);
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char HexDigit(std::uint32_t value, bool uppercase) {
    if (value < 10) return static_cast<char>('0' + value);
    return static_cast<char>((uppercase ? 'A' : 'a') + value - 10);
}

int ParseHex(const char* hex, std::size_t length, std::uint8_t* out, std::size_t capacity) {
    if ((length & 1) != 0) return NP_ERROR_INVALID_ARGUMENT;
    if (capacity < length / 2) return NP_ERROR_BUFFER_TOO_SMALL;
    for (std::size_t i = 0; i < length; i += 2) {
        const int high = HexValue(hex[i]);
        const int low = HexValue(hex[i + 1]);
        if (high < 0 || low < 0) return NP_ERROR_INVALID_ARGUMENT;
        out[i / 2] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return 0;
}

int FormatInteger(char* buf, std::size_t size, std::uint64_t magnitude, char sign, std::uint32_t radix,
    std::uint32_t flags, std::size_t* length) {
    if (radix < 1 || radix > NP_RADIX_MAX) return NP_ERROR_INVALID_ARGUMENT;
    char prefix[3] = {};
    std::size_t prefixLength = 0;
    if (sign != '\0') prefix[prefixLength++] = sign;
    if ((flags & NP_FORMAT_PREFIX) != 0 && (radix == 8 || radix == 16)) {
        prefix[prefixLength++] = '0';
        if (radix == 16) prefix[prefixLength++] = (flags & NP_FORMAT_UPPERCASE) != 0 ? 'X' : 'x';
    }
    std::size_t digits = 1;
    if (radix > 1) {
        for (std::uint64_t rest = magnitude; rest >= radix; rest /= radix) ++digits;
    }
    if (buf != nullptr) {
        if (size < prefixLength + digits + 1) return NP_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(buf, prefix, prefixLength);
        char* end = buf + prefixLength + digits;
        *end = '\0';
        std::uint64_t rest = magnitude;
        for (std::size_t i = 0; i < digits; ++i) {
            *--end = HexDigit(static_cast<std::uint32_t>(rest % radix), (flags & NP_FORMAT_UPPERCASE) != 0);
            rest /= radix;
        }
    }
    if (length != nullptr) *length = prefixLength + digits;
    return 0;
}

std::uint64_t Magnitude(std::int64_t value) {
    return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

char PlusSign(bool positive, std::uint32_t flags) {
    return positive && (flags & NP_FORMAT_SIGN) != 0 ? '+' : '\0';
}

template <typename Parse>
int ParseInteger(const char* str, std::size_t* parsedLength, Parse parse, bool* saturated) {
    char* end = nullptr;
    errno = 0;
    parse(str, &end);
    if (end == str) return NP_ERROR_OUT_OF_RANGE;
    if (*saturated && errno == ERANGE) return NP_ERROR_OUT_OF_RANGE;
    if (parsedLength != nullptr) *parsedLength = static_cast<std::size_t>(end - str);
    return 0;
}

int CopyToBuffer(char* dst, std::size_t size, const char* src, std::size_t length) {
    if (size < length + 1) return NP_ERROR_BUFFER_TOO_SMALL;
    std::strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
    return 0;
}

constexpr int NP_UTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550601);
constexpr int NP_UTIL_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80550602);
constexpr int NP_UTIL_ERROR_INVALID_CHARACTER = static_cast<int>(0x80550608);
constexpr std::size_t JSON_ESCAPE_MARGIN = 7;
constexpr std::size_t JSON_UNICODE_ESCAPE_LENGTH = 6;
constexpr std::size_t JSON_SURROGATE_PAIR_LENGTH = 12;
constexpr std::uint32_t SURROGATE_MASK = 0xFC00;
constexpr std::uint32_t HIGH_SURROGATE = 0xD800;
constexpr std::uint32_t LOW_SURROGATE = 0xDC00;
constexpr char URL_HEX_DIGITS[] = "0123456789ABCDEF";

const char* JsonShortEscape(unsigned char c) {
    switch (c) {
    case '\b': return "\\b";
    case '\t': return "\\t";
    case '\n': return "\\n";
    case '\f': return "\\f";
    case '\r': return "\\r";
    case '"': return "\\\"";
    case '/': return "\\/";
    case '\\': return "\\\\";
    default: return nullptr;
    }
}

char JsonShortUnescape(char c) {
    switch (c) {
    case '"': return '"';
    case '\\': return '\\';
    case '/': return '/';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    default: return '\0';
    }
}

int ParseUtf16Unit(const char* text, std::uint32_t* unit) {
    *unit = 0;
    for (int i = 0; i < 4; ++i) {
        const int value = HexValue(text[i]);
        if (value < 0) {
            *unit = 0;
            return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        }
        *unit = (*unit << 4) | static_cast<std::uint32_t>(value);
    }
    return 0;
}

std::size_t EncodeUtf8(std::uint32_t codePoint, char* out) {
    if (codePoint <= 0x7F) {
        out[0] = static_cast<char>(codePoint);
        return 1;
    }
    if (codePoint <= 0x7FF) {
        out[0] = static_cast<char>(0xC0 | (codePoint >> 6));
        out[1] = static_cast<char>(0x80 | (codePoint & 0x3F));
        return 2;
    }
    if (codePoint <= 0xFFFF) {
        out[0] = static_cast<char>(0xE0 | (codePoint >> 12));
        out[1] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (codePoint & 0x3F));
        return 3;
    }
    out[0] = static_cast<char>(0xF0 | (codePoint >> 18));
    out[1] = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (codePoint & 0x3F));
    return 4;
}

std::uint32_t CombineSurrogates(std::uint32_t high, std::uint32_t low) {
    return 0x10000 + (((high & 0x3FF) << 10) | (low & 0x3FF));
}

bool IsContinuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

std::size_t Utf8SequenceLength(unsigned char lead) {
    if (lead >= 0xC2 && lead <= 0xDF) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

bool IsValidSequence(const char* text, std::size_t length) {
    for (std::size_t i = 1; i < length; ++i) {
        if (!IsContinuation(text[i])) return false;
    }
    return true;
}

bool IsAllowedTextByte(unsigned char c) {
    return (c >= 0x20 && c != 0x7F) || c == '\t' || c == '\n' || c == '\r';
}

bool IsUrlSafe(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
        || c == '*';
}

} // namespace

extern "C" {

int APS5_VABI sceNpBase64GetDecodeSize(const char* src, std::size_t length) {
    return Base64Decode(src, length, nullptr, 0, BASE64_ALPHABET, true);
}

int APS5_VABI sceNpBase64Decoder(const char* src, std::size_t length, void* dst, std::size_t capacity) {
    return Base64Decode(src, length, static_cast<std::uint8_t*>(dst), capacity, BASE64_ALPHABET, true);
}

int APS5_VABI sceNpBase64Encoder(const void* src, char* dst, std::size_t length) {
    return Base64Encode(static_cast<const std::uint8_t*>(src), dst, length, BASE64_ALPHABET, true);
}

int APS5_VABI sceNpBase64UrlGetDecodeSize(const char* src, std::size_t length) {
    return Base64Decode(src, length, nullptr, 0, BASE64URL_ALPHABET, false);
}

int APS5_VABI sceNpBase64UrlDecoder(const char* src, std::size_t length, void* dst, std::size_t capacity) {
    return Base64Decode(src, length, static_cast<std::uint8_t*>(dst), capacity, BASE64URL_ALPHABET, false);
}

int APS5_VABI sceNpBase64UrlEncoder(const void* src, char* dst, std::size_t length) {
    return Base64Encode(static_cast<const std::uint8_t*>(src), dst, length, BASE64URL_ALPHABET, false);
}

int APS5_VABI sceNpHexToInt(int c) {
    return HexValue(static_cast<char>(c));
}

int APS5_VABI sceNpIntToHex(std::uint32_t value, bool uppercase) {
    return HexDigit(value, uppercase);
}

int APS5_VABI sceNpUInt32ToStr(char* buf, std::size_t size, std::uint32_t value, std::uint32_t radix, std::uint32_t flags,
    std::size_t* length) {
    return FormatInteger(buf, size, value, PlusSign(value != 0, flags), radix, flags, length);
}

int APS5_VABI sceNpUInt64ToStr(char* buf, std::size_t size, std::uint64_t value, std::uint32_t radix, std::uint32_t flags,
    std::size_t* length) {
    return FormatInteger(buf, size, value, PlusSign(value != 0, flags), radix, flags, length);
}

int APS5_VABI sceNpInt32ToStr(char* buf, std::size_t size, std::int32_t value, std::uint32_t radix, std::uint32_t flags,
    std::size_t* length) {
    const char sign = value < 0 ? '-' : PlusSign(value > 0, flags);
    return FormatInteger(buf, size, Magnitude(value), sign, radix, flags, length);
}

int APS5_VABI sceNpInt64ToStr(char* buf, std::size_t size, std::int64_t value, std::uint32_t radix, std::uint32_t flags,
    std::size_t* length) {
    const char sign = (flags & NP_FORMAT_SIGN) == 0 ? '\0' : value < 0 ? '-' : PlusSign(value > 0, flags);
    return FormatInteger(buf, size, Magnitude(value), sign, radix, flags, length);
}

int APS5_VABI sceNpStrToInt32(const char* str, std::int32_t* value, int base, std::size_t* parsedLength) {
    long long parsed = 0;
    bool saturated = false;
    const int result = ParseInteger(str, parsedLength, [&](const char* s, char** end) {
        parsed = std::strtoll(s, end, base);
        saturated = parsed == LLONG_MIN || parsed == LLONG_MAX;
    }, &saturated);
    if (result < 0) return result;
    if (parsed != static_cast<std::int32_t>(parsed)) return NP_ERROR_OUT_OF_RANGE;
    *value = static_cast<std::int32_t>(parsed);
    return 0;
}

int APS5_VABI sceNpStrToInt64(const char* str, std::int64_t* value, int base, std::size_t* parsedLength) {
    long long parsed = 0;
    bool saturated = false;
    const int result = ParseInteger(str, parsedLength, [&](const char* s, char** end) {
        parsed = std::strtoll(s, end, base);
        saturated = parsed == LLONG_MIN || parsed == LLONG_MAX;
    }, &saturated);
    if (result < 0) return result;
    *value = parsed;
    return 0;
}

int APS5_VABI sceNpStrToUInt32(const char* str, std::uint32_t* value, int base, std::size_t* parsedLength) {
    unsigned long long parsed = 0;
    bool saturated = false;
    const int result = ParseInteger(str, parsedLength, [&](const char* s, char** end) {
        parsed = std::strtoull(s, end, base);
        saturated = parsed == ULLONG_MAX;
    }, &saturated);
    if (result < 0) return result;
    if ((parsed >> 32) != 0) return NP_ERROR_OUT_OF_RANGE;
    *value = static_cast<std::uint32_t>(parsed);
    return 0;
}

int APS5_VABI sceNpStrToUInt64(const char* str, std::uint64_t* value, int base, std::size_t* parsedLength) {
    unsigned long long parsed = 0;
    bool saturated = false;
    const int result = ParseInteger(str, parsedLength, [&](const char* s, char** end) {
        parsed = std::strtoull(s, end, base);
        saturated = parsed == ULLONG_MAX;
    }, &saturated);
    if (result < 0) return result;
    *value = parsed;
    return 0;
}

int APS5_VABI sceNpStrBuildHex(char* dst, std::size_t size, const void* src, std::size_t length, bool uppercase) {
    if (size <= length * 2) return NP_ERROR_BUFFER_TOO_SMALL;
    const auto* bytes = static_cast<const std::uint8_t*>(src);
    for (std::size_t i = 0; i < length; ++i) {
        dst[i * 2] = HexDigit(bytes[i] >> 4, uppercase);
        dst[i * 2 + 1] = HexDigit(bytes[i] & 0xF, uppercase);
    }
    dst[length * 2] = '\0';
    return 0;
}

int APS5_VABI sceNpStrParseHex(const char* hex, void* out, std::size_t capacity) {
    return ParseHex(hex, std::strlen(hex), static_cast<std::uint8_t*>(out), capacity);
}

int APS5_VABI sceNpStrnParseHex(const char* hex, std::size_t length, void* out, std::size_t capacity) {
    return ParseHex(hex, length, static_cast<std::uint8_t*>(out), capacity);
}

int APS5_VABI sceNpStrcpyToBuf(char* dst, std::size_t size, const char* src) {
    return CopyToBuffer(dst, size, src, std::strlen(src));
}

int APS5_VABI sceNpStrncpyToBuf(char* dst, std::size_t size, const char* src, std::size_t maxLength) {
    return CopyToBuffer(dst, size, src, strnlen(src, maxLength));
}

int APS5_VABI sceNpUtilJsonEscape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength) {
    if (dst == nullptr || dstSize == 0 || src == nullptr || srcLength == 0) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (dstSize < JSON_ESCAPE_MARGIN) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    std::size_t written = 0;
    for (std::size_t i = 0; i < srcLength; ++i) {
        if (written > dstSize - JSON_ESCAPE_MARGIN) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        const auto c = static_cast<unsigned char>(src[i]);
        if (c == 0) return 0;
        char piece[JSON_ESCAPE_MARGIN] = {};
        if (const char* escape = JsonShortEscape(c)) {
            std::strcpy(piece, escape);
        } else if (c >= 0x20 && c != 0x7F) {
            piece[0] = static_cast<char>(c);
        } else {
            std::snprintf(piece, sizeof(piece), "\\u%04x", c);
        }
        std::strcat(dst, piece);
        written += std::strlen(piece);
    }
    return 0;
}

int APS5_VABI sceNpUtilJsonUnescape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength,
    std::size_t* dstLength) {
    if (src == nullptr || srcLength == 0) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (dst == nullptr && dstLength == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    const char* end = src + srcLength;
    std::size_t count = 0;
    int status = 0;
    char* out = dst;
    auto emit = [&](const char* bytes, std::size_t length, std::size_t consumed) {
        if (out != nullptr) {
            std::memcpy(out, bytes, length);
            out += length;
        }
        count += length;
        src += consumed;
    };
    while (src < end) {
        if (dst != nullptr && count + 1 > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        const std::size_t remaining = static_cast<std::size_t>(end - src);
        if (src[0] != '\\' || remaining < 2) {
            emit(src, 1, 1);
            continue;
        }
        if (const char simple = JsonShortUnescape(src[1])) {
            emit(&simple, 1, 2);
            continue;
        }
        if (src[1] != 'u' || remaining < JSON_UNICODE_ESCAPE_LENGTH) {
            emit(src, 1, 1);
            continue;
        }
        std::uint32_t unit = 0;
        int result = ParseUtf16Unit(src + 2, &unit);
        if (result < 0) {
            status = result;
            emit(src, 1, 1);
            continue;
        }
        status = 0;
        char encoded[4] = {};
        if ((unit & SURROGATE_MASK) != HIGH_SURROGATE) {
            const std::size_t length = EncodeUtf8(unit, encoded);
            if (dst != nullptr && count + length > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
            emit(encoded, length, JSON_UNICODE_ESCAPE_LENGTH);
            continue;
        }
        std::uint32_t low = 0;
        if (remaining < JSON_SURROGATE_PAIR_LENGTH || src[6] != '\\' || src[7] != 'u') {
            emit(src, 1, 1);
            continue;
        }
        result = ParseUtf16Unit(src + 8, &low);
        if (result < 0) {
            status = result;
            emit(src, 1, 1);
            continue;
        }
        if ((low & SURROGATE_MASK) != LOW_SURROGATE) {
            emit(src, 1, 1);
            continue;
        }
        const std::size_t length = EncodeUtf8(CombineSurrogates(unit, low), encoded);
        if (dst != nullptr && count + length > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        emit(encoded, length, JSON_SURROGATE_PAIR_LENGTH);
    }
    ++count;
    if (count > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    if (dstLength != nullptr) *dstLength = count;
    if (out != nullptr) *out = '\0';
    return status;
}

int APS5_VABI sceNpUtilJsonGetOneChar(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength,
    std::size_t* dstLength, std::size_t* consumed) {
    if (src == nullptr || srcLength == 0) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (dst == nullptr && dstLength == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    std::size_t decoded = 1;
    std::size_t used = 1;
    int status = 0;
    char encoded[4] = {};
    const auto lead = static_cast<unsigned char>(src[0]);
    if (lead == '\\') {
        used = 2;
        if (srcLength < 2) {
            status = NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        } else if (const char simple = JsonShortUnescape(src[1])) {
            encoded[0] = simple;
        } else if (src[1] != 'u') {
            used = 1;
            encoded[0] = '\\';
        } else {
            used = JSON_UNICODE_ESCAPE_LENGTH;
            std::uint32_t unit = 0;
            if (srcLength < JSON_UNICODE_ESCAPE_LENGTH || ParseUtf16Unit(src + 2, &unit) < 0) {
                status = NP_UTIL_ERROR_BUFFER_TOO_SMALL;
            } else if ((unit & SURROGATE_MASK) != HIGH_SURROGATE) {
                decoded = EncodeUtf8(unit, encoded);
            } else {
                used = JSON_SURROGATE_PAIR_LENGTH;
                std::uint32_t low = 0;
                if (srcLength < JSON_SURROGATE_PAIR_LENGTH || ParseUtf16Unit(src + 8, &low) < 0
                    || (low & SURROGATE_MASK) != LOW_SURROGATE) {
                    status = NP_UTIL_ERROR_BUFFER_TOO_SMALL;
                } else {
                    decoded = EncodeUtf8(CombineSurrogates(unit, low), encoded);
                }
            }
        }
    } else if (const std::size_t length = Utf8SequenceLength(lead); length != 0 && srcLength < length) {
        decoded = 0;
        used = length;
        status = NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    } else if (length != 0 && IsValidSequence(src, length)) {
        decoded = length;
        used = length;
        std::memcpy(encoded, src, length);
    } else {
        encoded[0] = src[0];
    }
    if (status == 0 && dst != nullptr && dstSize < decoded) status = NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    if (status == 0 && dst != nullptr) std::memcpy(dst, encoded, decoded);
    if (dstLength != nullptr) *dstLength = decoded;
    if (consumed != nullptr) *consumed = used;
    return status;
}

int APS5_VABI sceNpUtilGetNthChar(std::size_t index, const char* text, const char** character) {
    if (text == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    const char* position = text;
    for (std::size_t i = 0; i < index && *position != '\0'; ++i) {
        const auto lead = static_cast<unsigned char>(*position);
        std::size_t length = 1;
        if (lead < 0x80) {
            if (!IsAllowedTextByte(lead)) return NP_UTIL_ERROR_INVALID_CHARACTER;
        } else {
            length = Utf8SequenceLength(lead);
            if (length == 0 || !IsValidSequence(position, length)) return NP_UTIL_ERROR_INVALID_CHARACTER;
        }
        position += length;
    }
    *character = position;
    return 0;
}

int APS5_VABI sceNpUtilHttpUrlEncode(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength,
    std::size_t* dstLength) {
    if (src == nullptr || srcLength == 0) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (dst == nullptr && dstLength == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    std::size_t count = 0;
    for (std::size_t i = 0; i < srcLength; ++i) {
        const auto c = static_cast<unsigned char>(src[i]);
        const bool escaped = c != ' ' && !IsUrlSafe(c);
        const std::size_t length = escaped ? 3 : 1;
        if (dst != nullptr && count + length > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
        if (dst != nullptr) {
            if (escaped) {
                dst[count] = '%';
                dst[count + 1] = URL_HEX_DIGITS[c >> 4];
                dst[count + 2] = URL_HEX_DIGITS[c & 0xF];
            } else {
                dst[count] = c == ' ' ? '+' : static_cast<char>(c);
            }
        }
        count += length;
    }
    if (dst != nullptr && count + 1 > dstSize) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    if (dst != nullptr) dst[count] = '\0';
    if (dstLength != nullptr) *dstLength = count + 1;
    return 0;
}

}
