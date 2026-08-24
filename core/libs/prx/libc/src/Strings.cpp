#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <stdexcept>
#include <strings.h>

extern "C" {

void* memset_nid_postfix(void* s, int c, size_t n) {
    throw std::runtime_error("libc memset not implemented");
    return std::memset(s, c, n);
}

void* memcpy_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memcpy(dest, src, n);
}

void* memmove_nid_postfix(void* dest, const void* src, size_t n) {
    return std::memmove(dest, src, n);
}

int memcmp_nid_postfix(const void* s1, const void* s2, size_t n) {
    return std::memcmp(s1, s2, n);
}

const void* memchr_nid_postfix(const void* s, int c, size_t n) {
    return std::memchr(s, c, n);
}

int strcmp_nid_postfix(const char* s1, const char* s2) {
    return std::strcmp(s1, s2);
}

int strncmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    return std::strncmp(s1, s2, n);
}

int strcasecmp_nid_postfix(const char* s1, const char* s2) {
    return ::strcasecmp(s1, s2);
}

int strncasecmp_nid_postfix(const char* s1, const char* s2, size_t n) {
    return ::strncasecmp(s1, s2, n);
}

size_t strlen_nid_postfix(const char* s) {
    return std::strlen(s);
}

char* strcpy_nid_postfix(char* dest, const char* src) {
    return std::strcpy(dest, src);
}

char* strncpy_nid_postfix(char* dest, const char* src, size_t count) {
    return std::strncpy(dest, src, count);
}

char* strcat_nid_postfix(char* dest, const char* src) {
    return std::strcat(dest, src);
}

const char* strchr_nid_postfix(const char* s, int c) {
    return std::strchr(s, c);
}

char* strrchr_nid_postfix(const char* s, int c) {
    return std::strrchr(const_cast<char*>(s), c);
}

char* strstr_nid_postfix(const char* haystack, const char* needle) {
    return std::strstr(const_cast<char*>(haystack), needle);
}

char* strdup_nid_postfix(const char* s) {
    return ::strdup(s);
}

size_t strlcpy_nid_postfix(char* dest, const char* src, size_t size) {
    const size_t srcLen = std::strlen(src);
    if (size != 0u) {
        const size_t copyLen = srcLen < size - 1u ? srcLen : size - 1u;
        std::memcpy(dest, src, copyLen);
        dest[copyLen] = '\0';
    }
    return srcLen;
}

long strtol_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtol(str, endptr, base);
}

unsigned long strtoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

long long strtoll_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoll(str, endptr, base);
}

unsigned long long strtoull_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoull(str, endptr, base);
}

double strtod_nid_postfix(const char* str, char** endptr) {
    return std::strtod(str, endptr);
}

int atoi_nid_postfix(const char* str) {
    return std::atoi(str);
}

const wchar_t* wmemchr_nid_postfix(const wchar_t* s, wchar_t c, size_t n) {
    return std::wmemchr(s, c, n);
}

int wmemcmp_nid_postfix(const wchar_t* s1, const wchar_t* s2, size_t n) {
    return std::wmemcmp(s1, s2, n);
}

wchar_t* wmemcpy_nid_postfix(wchar_t* dest, const wchar_t* src, size_t n) {
    return std::wmemcpy(dest, src, n);
}

wchar_t* wmemmove_nid_postfix(wchar_t* dest, const wchar_t* src, size_t n) {
    return std::wmemmove(dest, src, n);
}

}
