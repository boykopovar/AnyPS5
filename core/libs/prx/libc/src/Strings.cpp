#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <string>

#include "prx/libc/include/General.hpp"

namespace {

template <typename Result, typename Parser>
Result ParseWide(const std::uint16_t* text, std::uint16_t** end, Parser parser) {
    std::string ascii;
    for (auto* cursor = text; *cursor != 0 && *cursor <= 0x7f; ++cursor)
        ascii.push_back(static_cast<char>(*cursor));
    char* parsedEnd = nullptr;
    const Result result = parser(ascii.c_str(), &parsedEnd);
    if (end != nullptr)
        *end = const_cast<std::uint16_t*>(text + (parsedEnd - ascii.c_str()));
    return result;
}

}

extern "C" {

void* APS5_VABI memset_nid_postfix(void* s, int c, size_t n) {
    // APS5_LOG_OUT("s=%p c=%d n=%zu", s, c, n);
    return std::memset(s, c, n);
}

void* APS5_VABI memcpy_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memcpy(dest, src, n);
}

void* APS5_VABI memmove_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memmove(dest, src, n);
}

int APS5_VABI memcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
    return std::memcmp(s1, s2, n);
}

const void* APS5_VABI memchr_nid_postfix(const void* s, int c, size_t n) {
    return std::memchr(s, c, n);
}

int APS5_VABI strcmp_nid_postfix(const char* s1, const char* s2) {
    return std::strcmp(s1, s2);
}

int APS5_VABI strncmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    return std::strncmp(s1, s2, n);
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

char* APS5_VABI strrchr_nid_postfix(const char* s, int c) {
    return std::strrchr(const_cast<char*>(s), c);
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

long APS5_VABI strtol_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtol(str, endptr, base);
}

unsigned long APS5_VABI strtoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

long long APS5_VABI strtoll_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoll(str, endptr, base);
}

unsigned long long APS5_VABI strtoull_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoull(str, endptr, base);
}

double APS5_VABI strtod_nid_postfix(const char* str, char** endptr) {
    return std::strtod(str, endptr);
}

double APS5_VABI atof_nid_postfix(const char* str) { return std::atof(str); }
float APS5_VABI strtof_nid_postfix(const char* str, char** endptr) { return std::strtof(str, endptr); }
long double APS5_VABI strtold_nid_postfix(const char* str, char** endptr) {
    static_assert(sizeof(long double) == 16, "Guest long double requires x87 extended precision storage");
    return std::strtold(str, endptr);
}

int APS5_VABI atoi_nid_postfix(const char* str) {
    return std::atoi(str);
}

// The PS5 SDK defines wchar_t as a 16-bit unsigned code unit under __SCE__.
// Use explicit guest-width elements rather than the host's wchar_t (32-bit on Linux).
size_t APS5_VABI wcslen_nid_postfix(const std::uint16_t* text) {
    size_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

int APS5_VABI wcscmp_nid_postfix(const std::uint16_t* left, const std::uint16_t* right) {
    while (*left != 0 && *left == *right) { ++left; ++right; }
    return *left < *right ? -1 : *left > *right ? 1 : 0;
}

int APS5_VABI wcsncmp_nid_postfix(const std::uint16_t* left, const std::uint16_t* right, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (left[index] != right[index]) return left[index] < right[index] ? -1 : 1;
        if (left[index] == 0) break;
    }
    return 0;
}

std::uint16_t* APS5_VABI wcscpy_nid_postfix(std::uint16_t* destination, const std::uint16_t* source) {
    auto* result = destination;
    while ((*destination++ = *source++) != 0) {}
    return result;
}

std::uint16_t* APS5_VABI wcsncpy_nid_postfix(std::uint16_t* destination, const std::uint16_t* source, size_t count) {
    size_t index = 0;
    for (; index < count && source[index] != 0; ++index) destination[index] = source[index];
    for (; index < count; ++index) destination[index] = 0;
    return destination;
}

std::uint16_t* APS5_VABI wcscat_nid_postfix(std::uint16_t* destination, const std::uint16_t* source) {
    wcscpy_nid_postfix(destination + wcslen_nid_postfix(destination), source);
    return destination;
}

std::uint16_t* APS5_VABI wcsncat_nid_postfix(std::uint16_t* destination, const std::uint16_t* source, size_t count) {
    auto* end = destination + wcslen_nid_postfix(destination);
    size_t index = 0;
    for (; index < count && source[index] != 0; ++index) end[index] = source[index];
    end[index] = 0;
    return destination;
}

const std::uint16_t* APS5_VABI wcschr_nid_postfix(const std::uint16_t* text, std::uint16_t character) {
    do { if (*text == character) return text; } while (*text++ != 0);
    return nullptr;
}

const std::uint16_t* APS5_VABI wcsrchr_nid_postfix(const std::uint16_t* text, std::uint16_t character) {
    const std::uint16_t* found = nullptr;
    do { if (*text == character) found = text; } while (*text++ != 0);
    return found;
}

const std::uint16_t* APS5_VABI wcsstr_nid_postfix(const std::uint16_t* text, const std::uint16_t* needle) {
    if (*needle == 0) return text;
    for (; *text != 0; ++text) {
        if (*text == *needle && wcsncmp_nid_postfix(text, needle, wcslen_nid_postfix(needle)) == 0)
            return text;
    }
    return nullptr;
}

const std::uint16_t* APS5_VABI wmemchr_nid_postfix(const std::uint16_t* text, std::uint16_t character, size_t count) {
    for (size_t index = 0; index < count; ++index) if (text[index] == character) return text + index;
    return nullptr;
}

int APS5_VABI wmemcmp_nid_postfix(const std::uint16_t* left, const std::uint16_t* right, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (left[index] != right[index]) return left[index] < right[index] ? -1 : 1;
    return 0;
}

std::uint16_t* APS5_VABI wmemcpy_nid_postfix(std::uint16_t* destination, const std::uint16_t* source, size_t count) {
    return static_cast<std::uint16_t*>(std::memcpy(destination, source, count * sizeof(std::uint16_t)));
}

std::uint16_t* APS5_VABI wmemmove_nid_postfix(std::uint16_t* destination, const std::uint16_t* source, size_t count) {
    return static_cast<std::uint16_t*>(std::memmove(destination, source, count * sizeof(std::uint16_t)));
}

}


extern "C" {

int APS5_VABI strcasecmp_nid_postfix(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        unsigned char a = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s1)));
        unsigned char b = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s2)));
        if (a != b) return a - b;
        ++s1; ++s2;
    }
    return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
}

int APS5_VABI strncasecmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s2) {
        unsigned char a = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s1)));
        unsigned char b = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s2)));
        if (a != b) return a - b;
        ++s1; ++s2; --n;
    }
    if (!n) return 0;
    return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
}

char* APS5_VABI strdup_nid_postfix(const char* s) {
    std::size_t len = std::strlen(s) + 1;
    char* copy = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(len));
    std::memcpy(copy, s, len);
    return copy;
}

int APS5_VABI bcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
    return std::memcmp(s1, s2, n);
}

size_t APS5_VABI strspn_nid_postfix(const char* s, const char* accept) {
    return std::strspn(s, accept);
}

int APS5_VABI strncpy_s_nid_postfix(char* dest, size_t destsz, const char* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    if (!dest || destsz == 0) return GuestEinval;
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
    if (!dest || destsz == 0) return GuestEinval;
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
    if (!dest) return GuestEinval;
    if (!src || count > destsz) {
        std::memset(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    std::memcpy(dest, src, count);
    return 0;
}

int APS5_VABI memmove_s_nid_postfix(void* dest, size_t destsz, const void* src, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    if (!dest) return GuestEinval;
    if (!src || count > destsz) {
        std::memset(dest, 0, destsz);
        return src ? GuestErange : GuestEinval;
    }
    std::memmove(dest, src, count);
    return 0;
}

int APS5_VABI memset_s_nid_postfix(void* dest, size_t destsz, int value, size_t count) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    if (!dest) return GuestEinval;
    const auto length = count > destsz ? destsz : count;
    auto* bytes = static_cast<volatile unsigned char*>(dest);
    for (size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(value);
    return count > destsz ? GuestErange : 0;
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
    return std::strtoull(str, endptr, base);
}

const std::uint16_t* APS5_VABI wcspbrk_nid_postfix(const std::uint16_t* text, const std::uint16_t* accept) {
    for (; *text != 0; ++text) {
        for (auto* character = accept; *character != 0; ++character)
            if (*text == *character) return text;
    }
    return nullptr;
}

size_t APS5_VABI wcsspn_nid_postfix(const std::uint16_t* text, const std::uint16_t* accept) {
    size_t length = 0;
    for (; text[length] != 0; ++length) {
        bool found = false;
        for (auto* character = accept; *character != 0; ++character)
            if (text[length] == *character) { found = true; break; }
        if (!found) break;
    }
    return length;
}

std::uint16_t* APS5_VABI wmemset_nid_postfix(std::uint16_t* text, std::uint16_t character, size_t count) {
    for (size_t index = 0; index < count; ++index) text[index] = character;
    return text;
}

double APS5_VABI wcstod_nid_postfix(const std::uint16_t* text, std::uint16_t** end) {
    return ParseWide<double>(text, end, [](const char* value, char** parsedEnd) {
        return std::strtod(value, parsedEnd);
    });
}

float APS5_VABI wcstof_nid_postfix(const std::uint16_t* text, std::uint16_t** end) {
    return ParseWide<float>(text, end, [](const char* value, char** parsedEnd) {
        return std::strtof(value, parsedEnd);
    });
}

long long APS5_VABI wcstol_nid_postfix(const std::uint16_t* text, std::uint16_t** end, int base) {
    return ParseWide<long long>(text, end, [base](const char* value, char** parsedEnd) {
        return std::strtoll(value, parsedEnd, base);
    });
}

long long APS5_VABI wcstoll_nid_postfix(const std::uint16_t* text, std::uint16_t** end, int base) {
    return ParseWide<long long>(text, end, [base](const char* value, char** parsedEnd) {
        return std::strtoll(value, parsedEnd, base);
    });
}

}
