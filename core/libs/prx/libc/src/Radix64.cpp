/* Derived from FreeBSD lib/libc/stdlib/a64l.c and lib/libc/stdlib/l64a.c (8c639945a445) */
/*-
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>

namespace {

constexpr char kRadix64Table[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

struct L64aBufferTag {};

constexpr int DecodeDigit(signed char c) {
    if (c <= '/') return c - '/' + 1;
    if (c <= '9') return c - '0' + 2;
    if (c <= 'Z') return c - 'A' + 12;
    return c - 'a' + 38;
}

}

extern "C" std::int64_t APS5_VABI a64l_nid_postfix(const char* s) {
    if (!s) return 0;
    std::uint32_t value = 0;
    int shift = 0;
    for (int i = 0; i < 6 && s[i] != '\0'; ++i, shift += 6) {
        value |= static_cast<std::uint32_t>(DecodeDigit(static_cast<signed char>(s[i]))) << shift;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(value));
}

extern "C" int APS5_VABI l64a_r_nid_postfix(std::int64_t value, char* buffer, int buflen) {
    if (!buffer || buflen <= 0) return -1;
    auto v = static_cast<std::uint32_t>(value);
    while (buflen-- > 0) {
        if (v == 0) {
            *buffer = '\0';
            return 0;
        }
        *buffer++ = kRadix64Table[v & 0x3f];
        v >>= 6;
    }
    return -1;
}

extern "C" char* APS5_VABI l64a_nid_postfix(std::int64_t value) {
    auto& buf = HostThreadLocal<std::array<char, 7>, L64aBufferTag>();
    l64a_r_nid_postfix(value, buf.data(), static_cast<int>(buf.size()));
    return buf.data();
}
