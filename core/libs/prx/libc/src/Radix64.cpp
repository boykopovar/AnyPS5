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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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
    const auto baseAddr = reinterpret_cast<std::uintptr_t>(s);
    for (int i = 0; i < 6; ++i, shift += 6) {
        const auto offset = static_cast<std::size_t>(i);
        if (baseAddr > UINTPTR_MAX - offset) return 0;
        char c = '\0';
        if (!GuestCopyIn_nid_no_patch(&c, s + i, sizeof(c))) return 0;
        if (c == '\0') break;
        value |= static_cast<std::uint32_t>(DecodeDigit(static_cast<signed char>(c))) << shift;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(value));
}

extern "C" int APS5_VABI l64a_r_nid_postfix(std::int64_t value, char* buffer, int buflen) {
    if (!buffer || buflen <= 0) return -1;
    std::array<char, 7> staged{};
    std::size_t writeCount = 0;
    bool terminated = false;
    auto v = static_cast<std::uint32_t>(value);
    const std::size_t maxWrite = static_cast<std::size_t>(buflen) < staged.size()
        ? static_cast<std::size_t>(buflen)
        : staged.size();
    while (writeCount < maxWrite) {
        const char c = v == 0 ? '\0' : kRadix64Table[v & 0x3f];
        staged[writeCount++] = c;
        if (v == 0) {
            terminated = true;
            break;
        }
        v >>= 6;
    }

    const auto baseAddr = reinterpret_cast<std::uintptr_t>(buffer);
    if (baseAddr > UINTPTR_MAX - (writeCount - 1)) return -1;

    char probeTail = '\0';
    if (!GuestCopyIn_nid_no_patch(&probeTail, buffer + writeCount - 1, sizeof(probeTail)) ||
        !GuestCopyOut_nid_no_patch(buffer + writeCount - 1, &probeTail, sizeof(probeTail))) {
        return -1;
    }
    if (writeCount > 1) {
        char probeHead = '\0';
        if (!GuestCopyIn_nid_no_patch(&probeHead, buffer, sizeof(probeHead)) ||
            !GuestCopyOut_nid_no_patch(buffer, &probeHead, sizeof(probeHead))) {
            return -1;
        }
    }

    if (!GuestCopyOut_nid_no_patch(buffer, staged.data(), writeCount)) return -1;
    return terminated ? 0 : -1;
}

extern "C" char* APS5_VABI l64a_nid_postfix(std::int64_t value) {
    auto& buf = HostThreadLocal<std::array<char, 7>, L64aBufferTag>();
    l64a_r_nid_postfix(value, buf.data(), static_cast<int>(buf.size()));
    return buf.data();
}
