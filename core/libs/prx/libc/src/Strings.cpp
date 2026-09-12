#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cwchar>
#include <cstdio>

#include "prx/libc/include/General.hpp"

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

int APS5_VABI atoi_nid_postfix(const char* str) {
    return std::atoi(str);
}

const wchar_t* APS5_VABI wmemchr_nid_postfix(const wchar_t* s, wchar_t c, size_t n) {
    return std::wmemchr(s, c, n);
}

int APS5_VABI wmemcmp_nid_postfix(const wchar_t* s1, const wchar_t* s2, size_t n) {
    return std::wmemcmp(s1, s2, n);
}

wchar_t* APS5_VABI wmemcpy_nid_postfix(wchar_t* dest, const wchar_t* src, size_t n) {
    return std::wmemcpy(dest, src, n);
}

wchar_t* APS5_VABI wmemmove_nid_postfix(wchar_t* dest, const wchar_t* src, size_t n) {
    return std::wmemmove(dest, src, n);
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
    char* copy = static_cast<char*>(std::malloc(len));
    if (!copy) return nullptr;
    std::memcpy(copy, s, len);
    return copy;
}

}
