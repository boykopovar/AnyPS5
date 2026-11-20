#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/urename.h"
#include "unicode/uvernum.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

extern "C" {

char16_t u_asciiToUpper(char16_t c);
char uprv_toupper(char c);
char uprv_asciitolower(char c);
char uprv_ebcdictolower(char c);
int uprv_stricmp(const char* str1, const char* str2);
int uprv_strnicmp(const char* str1, const char* str2, std::uint32_t n);
std::int8_t uprv_isInvariantString(const char* s, std::int32_t length);
std::int8_t uprv_isInvariantUString(const char16_t* s, std::int32_t length);

}

namespace {

constexpr std::size_t ChunkBytes = 4096;
constexpr std::uint64_t Unbounded = std::numeric_limits<std::uint64_t>::max();

std::size_t PageRemainder(const void* address) {
    return ChunkBytes - reinterpret_cast<std::uintptr_t>(address) % ChunkBytes;
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable string");
}

std::int32_t CompareIgnoringCase(const char* str1, const char* str2, std::uint64_t n, const char* function) {
    char first[ChunkBytes];
    char second[ChunkBytes];
    while (n != 0) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>({PageRemainder(str1), PageRemainder(str2), n}));
        CopyIn(first, str1, count, function);
        CopyIn(second, str2, count, function);
        const int result = uprv_strnicmp(first, second, static_cast<std::uint32_t>(count));
        if (result != 0 || std::memchr(first, 0, count)) return result;
        str1 += count;
        str2 += count;
        n -= count;
    }
    return 0;
}

template <typename Unit>
std::int8_t IsInvariant(const Unit* s, std::int32_t length, std::int8_t (*check)(const Unit*, std::int32_t),
                        const char* function) {
    Unit chunk[ChunkBytes / sizeof(Unit)];
    std::uint64_t remaining = length < 0 ? Unbounded : static_cast<std::uint64_t>(length);
    while (remaining != 0) {
        const auto units = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::max<std::size_t>(PageRemainder(s) / sizeof(Unit), 1), remaining));
        CopyIn(chunk, s, units * sizeof(Unit), function);
        if (length < 0) {
            const Unit* end = std::find(chunk, chunk + units, Unit{});
            if (end != chunk + units) return check(chunk, static_cast<std::int32_t>(end - chunk));
        }
        if (!check(chunk, static_cast<std::int32_t>(units))) return false;
        s += units;
        remaining -= units;
    }
    return true;
}

}

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t c) {
    return u_asciiToUpper(c);
}

char APS5_VABI uprv_toupper_nid_postfix(char c) {
    return uprv_toupper(c);
}

char APS5_VABI uprv_asciitolower_nid_postfix(char c) {
    return uprv_asciitolower(c);
}

char APS5_VABI uprv_ebcdictolower_nid_postfix(char c) {
    return uprv_ebcdictolower(c);
}

std::int32_t APS5_VABI uprv_stricmp_nid_postfix(const char* str1, const char* str2) {
    if (!str1 || !str2) return uprv_stricmp(str1, str2);
    return CompareIgnoringCase(str1, str2, Unbounded, __func__);
}

std::int32_t APS5_VABI uprv_strnicmp_nid_postfix(const char* str1, const char* str2, std::uint32_t n) {
    if (!str1 || !str2) return uprv_strnicmp(str1, str2, n);
    return CompareIgnoringCase(str1, str2, n, __func__);
}

std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* s, std::int32_t length) {
    if (!s && length != 0) APS5_INVALID_ARG_EX;
    return IsInvariant(s, length, uprv_isInvariantString, __func__);
}

std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* s, std::int32_t length) {
    if (!s && length != 0) APS5_INVALID_ARG_EX;
    return IsInvariant(s, length, uprv_isInvariantUString, __func__);
}

}
