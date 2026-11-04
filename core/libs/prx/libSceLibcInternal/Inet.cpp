/*-
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Adapted from FreeBSD release/11.4.0 lib/libc/inet/inet_network.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet/inet_network.c */
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int AddressFamilyInet = 2;
char g_ntoaBuffer[18];

bool IsDigit(unsigned char c) { return c >= '0' && c <= '9'; }
bool IsHexDigit(unsigned char c) { return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool IsSpace(unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
bool IsLower(unsigned char c) { return c >= 'a' && c <= 'z'; }

}

extern "C" {

int APS5_VABI inet_aton_nid_postfix(const char* text, std::uint32_t* address) {
    if (text == nullptr) throw std::invalid_argument("inet_aton: null text");
    std::uint64_t value = 0;
    std::uint8_t parts[4] = {};
    std::uint8_t* part = parts;
    bool digit = false;
    auto c = static_cast<unsigned char>(*text);
    for (;;) {
        if (!IsDigit(c)) return 0;
        value = 0;
        unsigned base = 10;
        digit = false;
        if (c == '0') {
            c = static_cast<unsigned char>(*++text);
            if (c == 'x' || c == 'X') {
                base = 16;
                c = static_cast<unsigned char>(*++text);
            } else {
                base = 8;
                digit = true;
            }
        }
        for (;;) {
            if (IsDigit(c)) {
                if (base == 8 && (c == '8' || c == '9')) return 0;
                value = value * base + (c - '0');
                c = static_cast<unsigned char>(*++text);
                digit = true;
            } else if (base == 16 && IsHexDigit(c)) {
                value = (value << 4) | static_cast<std::uint64_t>(c + 10 - (c >= 'a' ? 'a' : 'A'));
                c = static_cast<unsigned char>(*++text);
                digit = true;
            } else {
                break;
            }
        }
        if (c != '.') break;
        if (part >= parts + 3 || value > 0xff) return 0;
        *part++ = static_cast<std::uint8_t>(value);
        c = static_cast<unsigned char>(*++text);
    }
    if (c != 0 && (c >= 0x80 || !IsSpace(c))) return 0;
    if (!digit) return 0;
    switch (part - parts + 1) {
    case 2:
        if (value > 0xffffff) return 0;
        value |= static_cast<std::uint64_t>(parts[0]) << 24;
        break;
    case 3:
        if (value > 0xffff) return 0;
        value |= (static_cast<std::uint64_t>(parts[0]) << 24) | (static_cast<std::uint64_t>(parts[1]) << 16);
        break;
    case 4:
        if (value > 0xff) return 0;
        value |= (static_cast<std::uint64_t>(parts[0]) << 24) | (static_cast<std::uint64_t>(parts[1]) << 16) | (static_cast<std::uint64_t>(parts[2]) << 8);
        break;
    default:
        break;
    }
    if (address != nullptr) {
        const auto host = static_cast<std::uint32_t>(value);
        const std::uint8_t network[4] = {static_cast<std::uint8_t>(host >> 24), static_cast<std::uint8_t>(host >> 16), static_cast<std::uint8_t>(host >> 8), static_cast<std::uint8_t>(host)};
        std::memcpy(address, network, sizeof(network));
    }
    return 1;
}

std::uint32_t APS5_VABI inet_addr_nid_postfix(const char* text) {
    std::uint32_t address = 0;
    return inet_aton_nid_postfix(text, &address) ? address : 0xffffffffu;
}

char* APS5_VABI inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size) {
    __inet_ntop_nid_postfix(AddressFamilyInet, &address, buffer, size);

    return buffer;
}

char* APS5_VABI __inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size) {
    return inet_ntoa_r_nid_postfix(address, buffer, size);
}

char* APS5_VABI inet_ntoa_nid_postfix(std::uint32_t address) {
    std::memcpy(g_ntoaBuffer, "[inet_ntoa error]", sizeof(g_ntoaBuffer));
    __inet_ntop_nid_postfix(AddressFamilyInet, &address, g_ntoaBuffer, sizeof(g_ntoaBuffer));

    return g_ntoaBuffer;
}

std::uint32_t APS5_VABI inet_netof_nid_postfix(std::uint32_t address) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&address);
    const std::uint32_t host = (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
                               (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
    if ((host & 0x80000000u) == 0) return (host & 0xff000000u) >> 24;
    if ((host & 0xc0000000u) == 0x80000000u) return (host & 0xffff0000u) >> 16;
    return (host & 0xffffff00u) >> 8;
}

std::uint32_t APS5_VABI __inet_netof_nid_postfix(std::uint32_t address) {
    return inet_netof_nid_postfix(address);
}

std::uint32_t APS5_VABI inet_lnaof_nid_postfix(std::uint32_t address) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&address);
    const std::uint32_t host = (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
                               (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
    if ((host & 0x80000000u) == 0) return host & 0x00ffffffu;
    if ((host & 0xc0000000u) == 0x80000000u) return host & 0x0000ffffu;
    return host & 0x000000ffu;
}

std::uint32_t APS5_VABI __inet_lnaof_nid_postfix(std::uint32_t address) {
    return inet_lnaof_nid_postfix(address);
}

std::uint32_t APS5_VABI inet_makeaddr_nid_postfix(std::uint32_t net, std::uint32_t host) {
    std::uint32_t value = 0;
    if (net < 128) value = (net << 24) | (host & 0x00ffffffu);
    else if (net < 65536) value = (net << 16) | (host & 0x0000ffffu);
    else if (net < 16777216) value = (net << 8) | (host & 0x000000ffu);
    else value = net | host;
    std::uint32_t address = 0;
    const std::uint8_t network[4] = {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
                                     static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
    std::memcpy(&address, network, sizeof(network));
    return address;
}

std::uint32_t APS5_VABI __inet_makeaddr_nid_postfix(std::uint32_t net, std::uint32_t host) {
    return inet_makeaddr_nid_postfix(net, host);
}

std::uint32_t APS5_VABI inet_network_nid_postfix(const char* text) {
    if (text == nullptr) throw std::invalid_argument("inet_network: null text");
    std::uint32_t val = 0;
    std::uint32_t base = 10;
    std::uint32_t parts[4] = {};
    std::uint32_t* pp = parts;
    int digit = 0;

again:
    val = 0;
    base = 10;
    digit = 0;
    if (*text == '0') {
        digit = 1;
        base = 8;
        text++;
    }
    if (*text == 'x' || *text == 'X') {
        base = 16;
        text++;
    }
    char c = *text;
    while (c != '\0') {
        const auto uc = static_cast<unsigned char>(c);
        if (IsDigit(uc)) {
            if (base == 8 && (c == '8' || c == '9')) return 0xffffffffu;
            val = (val * base) + static_cast<std::uint32_t>(c - '0');
            text++;
            digit = 1;
            c = *text;
            continue;
        }
        if (base == 16 && IsHexDigit(uc)) {
            val = (val << 4) + static_cast<std::uint32_t>(uc + 10 - (IsLower(uc) ? 'a' : 'A'));
            text++;
            digit = 1;
            c = *text;
            continue;
        }
        break;
    }
    if (!digit) return 0xffffffffu;
    if (pp >= parts + 4 || val > 0xffu) return 0xffffffffu;
    if (*text == '.') {
        *pp++ = val;
        text++;
        goto again;
    }
    if (*text != '\0' && !IsSpace(static_cast<unsigned char>(*text))) return 0xffffffffu;
    *pp++ = val;
    const std::ptrdiff_t n = pp - parts;
    if (n > 4) return 0xffffffffu;
    val = 0;
    for (std::ptrdiff_t i = 0; i < n; i++) {
        val <<= 8;
        val |= parts[i] & 0xffu;
    }
    return val;
}

std::uint32_t APS5_VABI __inet_network_nid_postfix(const char* text) {
    return inet_network_nid_postfix(text);
}

}
