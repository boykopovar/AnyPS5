#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cwchar>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"

namespace {

int ByteDifference(const char* left, const char* right, std::size_t count, bool stopAtTerminator) {
    const auto* a = reinterpret_cast<const unsigned char*>(left);
    const auto* b = reinterpret_cast<const unsigned char*>(right);
    for (std::size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (stopAtTerminator && a[i] == 0) return 0;
    }
    return 0;
}

int CompareUnits(char16_t left, char16_t right) {
    return left < right ? -1 : 1;
}

bool Contains(const char16_t* set, char16_t character) {
    for (; *set != 0; ++set) {
        if (*set == character) return true;
    }
    return false;
}

size_t Length(const char16_t* s) {
    size_t length = 0;
    while (s[length] != 0) ++length;
    return length;
}

std::string AsciiPrefix(const char16_t* text) {
    std::string prefix;
    for (; *text != 0 && *text < 0x80; ++text) prefix.push_back(static_cast<char>(*text));
    return prefix;
}

struct NanSubject {
    const char* tail;
    bool negative;
};

NanSubject FindNanSubject(const char* text) {
    while (*text == ' ' || (*text >= '\t' && *text <= '\r')) ++text;
    bool negative = false;
    if (*text == '+' || *text == '-') negative = *text++ == '-';
    if ((text[0] | 0x20) == 'n' && (text[1] | 0x20) == 'a' && (text[2] | 0x20) == 'n') return {text + 3, negative};
    return {nullptr, negative};
}

unsigned HexNanDigit(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0' + 0x10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 0x1a;
    if (c >= 'A' && c <= 'F') return c - 'A' + 0x1a;
    return 0;
}

void ShiftNanWords(std::uint32_t* words, int from, int to, int digits) {
    const int shift = (8 - digits) * 4;
    const int carry = 32 - shift;
    int index = from;
    do {
        words[index] |= words[index + 1] << carry;
        words[index + 1] >>= shift;
    } while (++index < to);
}

std::uint64_t HexNanPayload(const char* open, const char*& end) {
    std::uint32_t words[2] = {};
    constexpr int high = 1;
    int top = high;
    int group = high;
    int digits = 0;
    int groupStart = 0;
    int inWord = 0;
    const auto* s = reinterpret_cast<const unsigned char*>(open);
    const auto skipBlanksAndPrefix = [&s](bool inside) {
        while (s[1] != 0 && s[1] <= ' ') ++s;
        if (inside && s[1] == 0) throw std::invalid_argument("strtod: blanks after a NAN payload digit reach the terminating NUL, where the console reads past it");
        if (s[1] == '0' && (s[2] == 'x' || s[2] == 'X') && s[3] > ' ') s += 2;
    };
    skipBlanksAndPrefix(false);
    for (unsigned char c; (c = *++s) != 0;) {
        const unsigned digit = HexNanDigit(c);
        if (digit == 0) {
            if (c <= ' ') {
                if (groupStart < digits) {
                    if (top < group && inWord < 8) ShiftNanWords(words, top, group, inWord);
                    if (top <= 0) {
                        inWord = 8;
                        continue;
                    }
                    groupStart = digits;
                    words[--top] = 0;
                    group = top;
                    inWord = 0;
                }
                skipBlanksAndPrefix(true);
                continue;
            }
            if (c == ')' && digits != 0) {
                end = reinterpret_cast<const char*>(s + 1);
                break;
            }
            for (; c != 0; c = *++s) {
                if (c == ')') {
                    end = reinterpret_cast<const char*>(s + 1);
                    break;
                }
            }
            return 0;
        }
        ++digits;
        if (++inWord > 8) {
            if (top <= 0) continue;
            inWord = 1;
            words[--top] = 0;
        }
        words[top] = (words[top] << 4) | (digit & 0xf);
    }
    if (digits == 0) return 0;
    if (top < group && inWord < 8) ShiftNanWords(words, top, group, inWord);
    if (top > 0) {
        int to = 0;
        int from = top;
        do words[to++] = words[from++];
        while (from <= high);
        do words[to++] = 0;
        while (to <= high);
    } else {
        words[high] &= 0xffffffffu >> 12;
    }
    if (words[0] == 0 && words[1] == 0) words[0] = 1;
    return (static_cast<std::uint64_t>(words[1]) << 32) | words[0];
}

const char* NanSequenceEnd(const char* tail) {
    if (*tail != '(') return tail;
    const char* cursor = tail + 1;
    while ((*cursor >= '0' && *cursor <= '9') || ((*cursor | 0x20) >= 'a' && (*cursor | 0x20) <= 'z') || *cursor == '_') ++cursor;
    return *cursor == ')' ? cursor + 1 : tail;
}

template<typename TFloat>
TFloat NanValue(const NanSubject& subject, const char*& end) {
    if constexpr (std::is_same_v<TFloat, double>) {
        end = subject.tail;
        const std::uint64_t payload = *subject.tail == '(' ? HexNanPayload(subject.tail, end) : 0;
        return std::bit_cast<double>((subject.negative ? 0x8000000000000000ull : 0) | 0x7ff8000000000000ull | payload);
    } else if constexpr (std::is_same_v<TFloat, float>) {
        end = NanSequenceEnd(subject.tail);
        return std::bit_cast<float>(0x7fc00000u);
    } else {
        end = NanSequenceEnd(subject.tail);
        TFloat value{};
        const std::uint64_t significand = 0xc000000000000000u;
        const std::uint16_t signExponent = 0x7fff;
        std::memcpy(&value, &significand, sizeof(significand));
        std::memcpy(reinterpret_cast<unsigned char*>(&value) + sizeof(significand), &signExponent, sizeof(signExponent));
        return value;
    }
}

template<typename TParse>
auto ParseAsciiPrefix(const char16_t* text, char16_t** end, TParse parse) {
    const std::string prefix = AsciiPrefix(text);
    char* parsedEnd = nullptr;
    const auto value = parse(prefix.c_str(), &parsedEnd);
    if (end != nullptr) *end = const_cast<char16_t*>(text) + (parsedEnd - prefix.c_str());
    return value;
}

template<typename TFloat>
TFloat ParseFloat(const char* text, char** end, TFloat (*parse)(const char*, char**)) {
    if (const NanSubject subject = FindNanSubject(text); subject.tail != nullptr) {
        const char* stop = subject.tail;
        const TFloat value = NanValue<TFloat>(subject, stop);
        if (end != nullptr) *end = const_cast<char*>(stop);
        return value;
    }
    char* parsedEnd = nullptr;
    const int saved = errno;
    const TFloat value = parse(text, &parsedEnd);
    if (errno == ERANGE && value != 0 && (value < 0 ? -value : value) < std::numeric_limits<TFloat>::min()) errno = saved;
    if (end != nullptr) *end = parsedEnd;
    return parsedEnd == text ? TFloat{0} : value;
}

}

extern "C" {

void* APS5_VABI memset_nid_postfix(void* s, int c, size_t n) {
    // APS5_LOG_OUT("s=%p c=%d n=%zu", s, c, n);
    return std::memset(s, c, n);
}

void APS5_VABI bzero_nid_postfix(void* destination, size_t count) {
    std::memset(destination, 0, count);
}

void APS5_VABI bcopy_nid_postfix(const void* source, void* destination, size_t count) {
    if (count != 0) std::memmove(destination, source, count);
}

void* APS5_VABI memcpy_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memcpy(dest, src, n);
}

void* APS5_VABI memmove_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memmove(dest, src, n);
}

int APS5_VABI memcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
    if (std::memcmp(s1, s2, n) == 0) return 0;
    return ByteDifference(static_cast<const char*>(s1), static_cast<const char*>(s2), n, false);
}

const void* APS5_VABI memchr_nid_postfix(const void* s, int c, size_t n) {
    return std::memchr(s, c, n);
}

void* APS5_VABI memrchr_nid_postfix(const void* s, int c, size_t n) {
    const auto* bytes = static_cast<const unsigned char*>(s);
    const auto value = static_cast<unsigned char>(c);
    while (n != 0) {
        --n;
        if (bytes[n] == value) return const_cast<unsigned char*>(bytes + n);
    }
    return nullptr;
}

void* APS5_VABI memccpy_nid_postfix(void* dest, const void* src, int c, size_t n) {
    auto* d = static_cast<unsigned char*>(dest);
    const auto* s = static_cast<const unsigned char*>(src);
    const auto target = static_cast<unsigned char>(c);
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
        if (s[i] == target) return d + i + 1;
    }
    return nullptr;
}

void* APS5_VABI memmem_nid_postfix(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
    if (needlelen == 0) return const_cast<void*>(haystack);
    if (haystacklen < needlelen) return nullptr;
    const auto* h = static_cast<const unsigned char*>(haystack);
    const auto* n = static_cast<const unsigned char*>(needle);
    const unsigned char first = n[0];
    const size_t limit = haystacklen - needlelen;
    for (size_t i = 0; i <= limit; ++i) {
        if (h[i] == first && std::memcmp(h + i, n, needlelen) == 0) {
            return const_cast<unsigned char*>(h + i);
        }
    }
    return nullptr;
}

int APS5_VABI strcmp_nid_postfix(const char* s1, const char* s2) {
    if (std::strcmp(s1, s2) == 0) return 0;
    return ByteDifference(s1, s2, static_cast<std::size_t>(-1), true);
}

int APS5_VABI strncmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    if (std::strncmp(s1, s2, n) == 0) return 0;
    return ByteDifference(s1, s2, n, true);
}

size_t APS5_VABI strlen_nid_postfix(const char* s) {
    return std::strlen(s);
}

char* APS5_VABI strcpy_nid_postfix(char* dest, const char* src) {
    return std::strcpy(dest, src);
}

char* APS5_VABI strncpy_nid_postfix(char* dest, const char* src, size_t count) {
    return std::strncpy(dest, src, count);
}

char* APS5_VABI strcat_nid_postfix(char* dest, const char* src) {
    return std::strcat(dest, src);
}

const char* APS5_VABI strchr_nid_postfix(const char* s, int c) {
    return std::strchr(s, c);
}

char* APS5_VABI index_nid_postfix(const char* s, int c) {
    return const_cast<char*>(strchr_nid_postfix(s, c));
}

char* APS5_VABI strrchr_nid_postfix(const char* s, int c) {
    return std::strrchr(const_cast<char*>(s), static_cast<char>(c));
}

char* APS5_VABI rindex_nid_postfix(const char* s, int c) {
    return strrchr_nid_postfix(s, c);
}

int APS5_VABI flsl_nid_postfix(std::int64_t mask) {
    return mask == 0 ? 0 : 64 - std::countl_zero(static_cast<std::uint64_t>(mask));
}

char* APS5_VABI strstr_nid_postfix(const char* haystack, const char* needle) {
    return std::strstr(const_cast<char*>(haystack), needle);
}

size_t APS5_VABI strlcpy_nid_postfix(char* dest, const char* src, size_t size) {
    const size_t srcLen = std::strlen(src);
    if (size != 0u) {
        const size_t copyLen = srcLen < size - 1u ? srcLen : size - 1u;
        std::memcpy(dest, src, copyLen);
        dest[copyLen] = '\0';
    }
    return srcLen;
}

bool StopAtPrefixLetter_nid_no_patch(const char* str, char** endptr, int base) {
    if (base != 0 && base != 2 && base != 16) return false;
    const char* const start = str;
    while (*str == ' ' || (*str >= '\t' && *str <= '\r')) ++str;
    if (*str == '+' || *str == '-') ++str;
    if (str[0] != '0' || str[1] == '\0') return false;
    const char next = str[2];
    const bool binary = base != 16 && (str[1] == 'b' || str[1] == 'B') && (next == '0' || next == '1');
    const bool hexDigit = (next >= '0' && next <= '9') || (next >= 'a' && next <= 'f') || (next >= 'A' && next <= 'F');
    const bool bareHex = base != 2 && (str[1] == 'x' || str[1] == 'X') && !hexDigit;
    if (!binary && !bareHex) return false;
    if (endptr) *endptr = const_cast<char*>(binary ? str + 1 : start);
    return true;
}

bool RejectIntegerBase_nid_no_patch(const char* str, char** endptr, int base, bool setError) {
    if (base == 0 || (base >= 2 && base <= 36)) return false;
    if (endptr) *endptr = const_cast<char*>(str);
    if (setError) errno = EINVAL;
    return true;
}

std::int64_t APS5_VABI strtol_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, true) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<long long>(str, endptr, base, std::strtoll);
}

std::uint64_t APS5_VABI strtoul_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, true) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<unsigned long long>(str, endptr, base, std::strtoull);
}

long long APS5_VABI strtoll_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, true) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<long long>(str, endptr, base, std::strtoll);
}

unsigned long long APS5_VABI strtoull_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, true) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<unsigned long long>(str, endptr, base, std::strtoull);
}

unsigned long long APS5_VABI strtouq_nid_postfix(const char* str, char** endptr, int base) {
    return strtoull_nid_postfix(str, endptr, base);
}

double APS5_VABI strtod_nid_postfix(const char* str, char** endptr) {
    return ParseFloat<double>(str, endptr, std::strtod);
}

double APS5_VABI atof_nid_postfix(const char* str) { return strtod_nid_postfix(str, nullptr); }
float APS5_VABI strtof_nid_postfix(const char* str, char** endptr) {
    return ParseFloat<float>(str, endptr, std::strtof);
}
long double APS5_VABI strtold_nid_postfix(const char* str, char** endptr) {
    static_assert(sizeof(long double) == 16, "Guest long double requires x87 extended precision storage");
    return ParseFloat<long double>(str, endptr, std::strtold);
}

int APS5_VABI atoi_nid_postfix(const char* str) {
    return static_cast<int>(strtoull_nid_postfix(str, nullptr, 10));
}

std::int64_t APS5_VABI atol_nid_postfix(const char* str) {
    return static_cast<std::int64_t>(strtoull_nid_postfix(str, nullptr, 10));
}

long long APS5_VABI atoll_nid_postfix(const char* str) {
    return static_cast<long long>(strtoull_nid_postfix(str, nullptr, 10));
}

std::div_t APS5_VABI div_nid_postfix(int numerator, int denominator) {
    return std::div(numerator, denominator);
}

const char16_t* APS5_VABI wmemchr_nid_postfix(const char16_t* s, char16_t c, size_t n) {
    for (; n != 0; ++s, --n) {
        if (*s == c) return s;
    }
    return nullptr;
}

int APS5_VABI wmemcmp_nid_postfix(const char16_t* s1, const char16_t* s2, size_t n) {
    for (; n != 0; ++s1, ++s2, --n) {
        if (*s1 != *s2) return CompareUnits(*s1, *s2);
    }
    return 0;
}

char16_t* APS5_VABI wmemcpy_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    if (n != 0) std::memcpy(dest, src, n * sizeof(char16_t));
    return dest;
}

char16_t* APS5_VABI wmemmove_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    if (n != 0) std::memmove(dest, src, n * sizeof(char16_t));
    return dest;
}

}


extern "C" {

int APS5_VABI tolower_nid_postfix(int);

int APS5_VABI strcasecmp_nid_postfix(const char* s1, const char* s2) {
    for (;; ++s1, ++s2) {
        const int a = tolower_nid_postfix(static_cast<unsigned char>(*s1));
        const int b = tolower_nid_postfix(static_cast<unsigned char>(*s2));
        if (a != b || *s1 == 0) return a - b;
    }
}

int APS5_VABI strncasecmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    for (; n != 0; ++s1, ++s2, --n) {
        const int a = tolower_nid_postfix(static_cast<unsigned char>(*s1));
        const int b = tolower_nid_postfix(static_cast<unsigned char>(*s2));
        if (a != b) return a - b;
        if (*s1 == 0) break;
    }
    return 0;
}

int APS5_VABI bcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
    return std::memcmp(s1, s2, n);
}

size_t APS5_VABI strspn_nid_postfix(const char* s, const char* accept) {
    return std::strspn(s, accept);
}

int APS5_VABI strcoll_nid_postfix(const char* s1, const char* s2) {
    return std::strcmp(s1, s2);
}

int APS5_VABI strncpy_s_nid_postfix(char* dest, size_t destsz, const char* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    if (!dest || destsz == 0) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src) {
        dest[0] = '\0';
        return GuestEinval;
    }
    size_t length = 0;
    while (length < count && src[length] != '\0') ++length;
    if (length >= destsz) {
        dest[0] = '\0';
        return GuestErange;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    if (destination < source + (length + 1) && source < destination + length + 1) {
        dest[0] = '\0';
        return GuestEinval;
    }
    std::memcpy(dest, src, length);
    dest[length] = '\0';
    return 0;
}

int APS5_VABI strcpy_s_nid_postfix(char* dest, size_t destsz, const char* src) {
    return strncpy_s_nid_postfix(dest, destsz, src, static_cast<size_t>(-1));
}

int APS5_VABI strncat_s_nid_postfix(char* dest, size_t destsz, const char* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    if (!dest || destsz == 0) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    size_t used = 0;
    while (used < destsz && dest[used] != '\0') ++used;
    if (used == destsz || !src) {
        dest[0] = '\0';
        return used == destsz ? GuestErange : GuestEinval;
    }
    size_t length = 0;
    while (length < count && src[length] != '\0') ++length;
    if (length >= destsz - used) {
        dest[0] = '\0';
        return GuestErange;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    if (destination < source + (length + 1) && source < destination + used + length + 1) {
        dest[0] = '\0';
        return GuestEinval;
    }
    std::memcpy(dest + used, src, length);
    dest[used + length] = '\0';
    return 0;
}

int APS5_VABI strcat_s_nid_postfix(char* dest, size_t destsz, const char* src) {
    return strncat_s_nid_postfix(dest, destsz, src, static_cast<size_t>(-1));
}

int APS5_VABI memcpy_s_nid_postfix(void* dest, size_t destsz, const void* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    if (!dest) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src || count > destsz) {
        std::memset(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    const auto distance = destination < source ? source - destination : destination - source;
    if (count != 0 && distance < count) {
        std::memset(dest, 0, destsz);
        return GuestEinval;
    }
    std::memcpy(dest, src, count);
    return 0;
}

int APS5_VABI memmove_s_nid_postfix(void* dest, size_t destsz, const void* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    if (!dest) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src || count > destsz) {
        std::memset(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    std::memmove(dest, src, count);
    return 0;
}

int APS5_VABI memset_s_nid_postfix(void* dest, size_t destsz, int value, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    if (!dest || destsz > RsizeMax || count > RsizeMax) return GuestEinval;
    const auto length = count > destsz ? destsz : count;
    auto* bytes = static_cast<volatile unsigned char*>(dest);
    for (size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(value);
    return count > destsz ? GuestEinval : 0;
}

char* APS5_VABI strnstr_nid_postfix(const char* haystack, const char* needle, size_t length) {
    const size_t needleLength = std::strlen(needle);
    if (needleLength == 0) return const_cast<char*>(haystack);
    for (size_t index = 0; index < length && haystack[index] != '\0'; ++index) {
        if (needleLength > length - index) break;
        if (std::strncmp(haystack + index, needle, needleLength) == 0) return const_cast<char*>(haystack + index);
    }
    return nullptr;
}

unsigned long long APS5_VABI _Stoull_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, false) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<unsigned long long>(str, endptr, base, std::strtoull);
}

size_t APS5_VABI wcslen_nid_postfix(const char16_t* s) {
    return Length(s);
}

size_t APS5_VABI wcsnlen_s_nid_postfix(const char16_t* s, size_t maxlen) {
    if (!s) return 0;
    size_t length = 0;
    while (length < maxlen && s[length] != 0) ++length;
    return length;
}

int APS5_VABI wcscmp_nid_postfix(const char16_t* s1, const char16_t* s2) {
    for (; *s1 == *s2; ++s1, ++s2) {
        if (*s1 == 0) return 0;
    }
    return CompareUnits(*s1, *s2);
}

int APS5_VABI wcsncmp_nid_postfix(const char16_t* s1, const char16_t* s2, size_t n) {
    for (; n != 0; ++s1, ++s2, --n) {
        if (*s1 != *s2) return CompareUnits(*s1, *s2);
        if (*s1 == 0) return 0;
    }
    return 0;
}

char16_t* APS5_VABI wcscpy_nid_postfix(char16_t* dest, const char16_t* src) {
    std::memcpy(dest, src, (Length(src) + 1) * sizeof(char16_t));
    return dest;
}

char16_t* APS5_VABI wcscat_nid_postfix(char16_t* dest, const char16_t* src) {
    std::memcpy(dest + Length(dest), src, (Length(src) + 1) * sizeof(char16_t));
    return dest;
}

char16_t* APS5_VABI wcsncpy_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    size_t index = 0;
    for (; index < n && src[index] != 0; ++index) dest[index] = src[index];
    for (; index < n; ++index) dest[index] = 0;
    return dest;
}

const char16_t* APS5_VABI wcschr_nid_postfix(const char16_t* s, char16_t c) {
    for (;; ++s) {
        if (*s == c) return s;
        if (*s == 0) return nullptr;
    }
}

const char16_t* APS5_VABI wcsrchr_nid_postfix(const char16_t* s, char16_t c) {
    const char16_t* found = nullptr;
    for (;; ++s) {
        if (*s == c) found = s;
        if (*s == 0) return found;
    }
}

const char16_t* APS5_VABI wcsstr_nid_postfix(const char16_t* haystack, const char16_t* needle) {
    const size_t length = Length(needle);
    if (length == 0) return haystack;
    for (; *haystack != 0; ++haystack) {
        size_t matched = 0;
        while (matched < length && haystack[matched] == needle[matched]) ++matched;
        if (matched == length) return haystack;
    }
    return nullptr;
}

const char16_t* APS5_VABI wcspbrk_nid_postfix(const char16_t* s, const char16_t* accept) {
    for (; *s != 0; ++s) {
        if (Contains(accept, *s)) return s;
    }
    return nullptr;
}

size_t APS5_VABI wcsspn_nid_postfix(const char16_t* s, const char16_t* accept) {
    size_t count = 0;
    while (s[count] != 0 && Contains(accept, s[count])) ++count;
    return count;
}

size_t APS5_VABI wcscspn_nid_postfix(const char16_t* s, const char16_t* reject) {
    size_t count = 0;
    while (s[count] != 0 && !Contains(reject, s[count])) ++count;
    return count;
}

char16_t* APS5_VABI wcstok_nid_postfix(char16_t* text, const char16_t* delimiters, char16_t** last) {
    if (text == nullptr) text = *last;
    if (text == nullptr) return nullptr;
    while (*text != 0 && Contains(delimiters, *text)) ++text;
    if (*text == 0) {
        *last = nullptr;
        return nullptr;
    }
    char16_t* end = text;
    while (*end != 0 && !Contains(delimiters, *end)) ++end;
    if (*end == 0) {
        *last = nullptr;
    } else {
        *end = 0;
        *last = end + 1;
    }
    return text;
}

char16_t* APS5_VABI wmemset_nid_postfix(char16_t* s, char16_t c, size_t n) {
    for (size_t index = 0; index < n; ++index) s[index] = c;
    return s;
}

double APS5_VABI wcstod_nid_postfix(const char16_t* str, char16_t** endptr) {
    return ParseAsciiPrefix(str, endptr, [](const char* text, char** end) { return ParseFloat<double>(text, end, std::strtod); });
}

float APS5_VABI wcstof_nid_postfix(const char16_t* str, char16_t** endptr) {
    return ParseAsciiPrefix(str, endptr, [](const char* text, char** end) { return ParseFloat<float>(text, end, std::strtof); });
}

long double APS5_VABI wcstold_nid_postfix(const char16_t* str, char16_t** endptr) {
    static_assert(sizeof(long double) == 16);
    static_assert(std::numeric_limits<long double>::digits == 64);
    return ParseAsciiPrefix(str, endptr, [](const char* text, char** end) { return ParseFloat<long double>(text, end, std::strtold); });
}

unsigned long long APS5_VABI wcstoul_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return ParseAsciiPrefix(str, endptr, [base](const char* text, char** end) { return RejectIntegerBase_nid_no_patch(text, end, base, false) || StopAtPrefixLetter_nid_no_patch(text, end, base) ? 0ULL : ParseInteger<unsigned long long>(text, end, base, std::strtoull); });
}

unsigned long long APS5_VABI wcstoull_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return ParseAsciiPrefix(str, endptr, [base](const char* text, char** end) { return RejectIntegerBase_nid_no_patch(text, end, base, false) || StopAtPrefixLetter_nid_no_patch(text, end, base) ? 0ULL : ParseInteger<unsigned long long>(text, end, base, std::strtoull); });
}

long long APS5_VABI wcstoll_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    const char16_t* digits = str;
    while (*digits == u' ' || (*digits >= u'\t' && *digits <= u'\r')) ++digits;
    const bool negative = *digits == u'-';
    if (*digits == u'+' || *digits == u'-') ++digits;
    const int saved = errno;
    errno = 0;
    char16_t* stop = nullptr;
    const unsigned long long magnitude = wcstoull_nid_postfix(digits, &stop, base);
    const bool overflowed = errno == ERANGE;
    errno = saved;
    if (stop == digits) {
        if (endptr) *endptr = const_cast<char16_t*>(str);
        return 0;
    }
    if (endptr) *endptr = stop;
    const unsigned long long limit = negative ? 0x8000000000000000ULL : 0x7fffffffffffffffULL;
    if (overflowed || magnitude > limit) {
        errno = ERANGE;
        return negative ? std::numeric_limits<long long>::min() : std::numeric_limits<long long>::max();
    }
    return static_cast<long long>(negative ? 0 - magnitude : magnitude);
}

long long APS5_VABI wcstol_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstoll_nid_postfix(str, endptr, base);
}

long long APS5_VABI wcstoimax_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstoll_nid_postfix(str, endptr, base);
}

unsigned long long APS5_VABI wcstoumax_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    return wcstoull_nid_postfix(str, endptr, base);
}

std::uint64_t APS5_VABI _WStoul_nid_postfix(const char16_t* str, char16_t** endptr, int base) {
    if (base < 0 || base == 1 || base > 36) throw std::invalid_argument("_WStoul: unsupported base " + std::to_string(base));
    return wcstoul_nid_postfix(str, endptr, base);
}

int APS5_VABI wcscoll_nid_postfix(const char16_t* first, const char16_t* second) {
    return wcscmp_nid_postfix(first, second);
}

size_t APS5_VABI wcsxfrm_nid_postfix(char16_t* destination, const char16_t* source, size_t count) {
    const size_t length = Length(source);
    if (count != 0) {
        const size_t copied = length < count ? length : count - 1;
        std::memcpy(destination, source, copied * sizeof(char16_t));
        destination[copied] = 0;
    }
    return length;
}

size_t APS5_VABI strxfrm_nid_postfix(char* destination, const char* source, size_t count) {
    return strlcpy_nid_postfix(destination, source, count);
}

char* APS5_VABI strtok_s_nid_postfix(char* str, const char* delim, char** context) {
    return SonyStrtokS(str, delim, context);
}

int APS5_VABI wmemcpy_s_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = (SIZE_MAX >> 1) / sizeof(char16_t);
    if (!dest) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src || count > destsz) {
        wmemset_nid_postfix(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    const auto distance = destination < source ? source - destination : destination - source;
    if (count != 0 && distance < count * sizeof(char16_t)) {
        wmemset_nid_postfix(dest, 0, destsz);
        return GuestEinval;
    }
    if (count != 0) std::memcpy(dest, src, count * sizeof(char16_t));
    return 0;
}

int APS5_VABI wmemmove_s_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = (SIZE_MAX >> 1) / sizeof(char16_t);
    if (!dest) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src || count > destsz) {
        wmemset_nid_postfix(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    if (count != 0) std::memmove(dest, src, count * sizeof(char16_t));
    return 0;
}

int APS5_VABI wcsncpy_s_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = (SIZE_MAX >> 1) / sizeof(char16_t);
    if (!dest || destsz == 0) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    if (!src) {
        dest[0] = 0;
        return GuestEinval;
    }
    size_t length = 0;
    while (length < count && src[length] != 0) ++length;
    if (length >= destsz) {
        dest[0] = 0;
        return GuestErange;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    const auto distance = destination < source ? source - destination : destination - source;
    if (length != 0 && distance < (length + 1) * sizeof(char16_t)) {
        dest[0] = 0;
        return GuestEinval;
    }
    std::memcpy(dest, src, length * sizeof(char16_t));
    dest[length] = 0;
    return 0;
}

char16_t* APS5_VABI wcsncat_nid_postfix(char16_t* dest, const char16_t* src, size_t n) {
    const size_t dest_len = Length(dest);
    size_t index = 0;
    for (; index < n && src[index] != 0; ++index) dest[dest_len + index] = src[index];
    dest[dest_len + index] = 0;
    return dest;
}

int APS5_VABI wcsncat_s_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr size_t RsizeMax = (SIZE_MAX >> 1) / sizeof(char16_t);
    if (!dest || destsz == 0) return GuestEinval;
    if (destsz > RsizeMax) return GuestErange;
    size_t used = 0;
    while (used < destsz && dest[used] != 0) ++used;
    if (used == destsz || !src) {
        dest[0] = 0;
        return used == destsz ? GuestErange : GuestEinval;
    }
    size_t length = 0;
    while (length < count && src[length] != 0) ++length;
    if (length >= destsz - used) {
        dest[0] = 0;
        return GuestErange;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest + used);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    const auto distance = destination < source ? source - destination : destination - source;
    if (length != 0 && distance < (length + 1) * sizeof(char16_t)) {
        dest[0] = 0;
        return GuestEinval;
    }
    std::memcpy(dest + used, src, length * sizeof(char16_t));
    dest[used + length] = 0;
    return 0;
}

int APS5_VABI wcscat_s_nid_postfix(char16_t* dest, size_t destsz, const char16_t* src) {
    return wcsncat_s_nid_postfix(dest, destsz, src, static_cast<size_t>(-1));
}

}
