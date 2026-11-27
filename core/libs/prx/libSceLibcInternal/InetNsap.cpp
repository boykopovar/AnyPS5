/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/inet/nsap_addr.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet/nsap_addr.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int GuestEfault = 14;

bool IsAscii(int value) { return value >= 0 && value < 128; }
bool IsDigit(int value) { return value >= '0' && value <= '9'; }
bool IsUpperHex(int value) { return value >= 'A' && value <= 'F'; }
bool IsXdigit(int value) { return IsDigit(value) || IsUpperHex(value) || (value >= 'a' && value <= 'f'); }
bool IsLower(int value) { return value >= 'a' && value <= 'z'; }
int ToUpper(int value) { return IsLower(value) ? value - ('a' - 'A') : value; }

std::uint8_t Xtob(int value) {
    return static_cast<std::uint8_t>(value - (IsDigit(value) ? '0' : '7'));
}

int GuestCharAt(const char* text, std::size_t index, bool& ok) {
    if (!ok) return 0;
    char value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, text + index, 1)) {
        ok = false;
        return 0;
    }
    return value;
}

}

extern "C" std::uint32_t APS5_VABI inet_nsap_addr_nid_postfix(const char* ascii, std::uint8_t* binary, int maxlen) {
    if (!ascii) throw std::runtime_error("inet_nsap_addr: null text");
    bool ok = true;
    if (GuestCharAt(ascii, 0, ok) != '0' || (GuestCharAt(ascii, 1, ok) != 'x' && GuestCharAt(ascii, 1, ok) != 'X')) {
        return 0;
    }
    std::size_t pos = 2;
    std::uint32_t length = 0;
    std::vector<std::uint8_t> out;
    const auto finish = [&](std::uint32_t result) -> std::uint32_t {
        if (!out.empty() && binary && !GuestCopyOut_nid_no_patch(binary, out.data(), out.size())) ok = false;
        if (!ok) {
            *__error_nid_postfix() = GuestEfault;
            return 0;
        }
        return result;
    };
    int c;
    while ((c = GuestCharAt(ascii, pos++, ok)) != 0 && length < static_cast<std::uint32_t>(maxlen)) {
        if (c == '.' || c == '+' || c == '/') continue;
        if (!IsAscii(c)) return finish(0);
        c = ToUpper(c);
        if (!IsXdigit(c)) return finish(0);
        const std::uint8_t nibble = Xtob(c);
        c = GuestCharAt(ascii, pos++, ok);
        if (c == 0) return finish(0);
        c = ToUpper(c);
        if (!IsXdigit(c)) return finish(0);
        out.push_back(static_cast<std::uint8_t>((nibble << 4) | Xtob(c)));
        length++;
    }
    return finish(length);
}

extern "C" char* APS5_VABI inet_nsap_ntoa_nid_postfix(int binlen, const std::uint8_t* binary, char* ascii) {
    static char tmpbuf[255 * 3 + 3];
    char* const start = ascii ? ascii : tmpbuf;
    if (binlen > 255) binlen = 255;
    std::vector<std::uint8_t> bytes;
    if (binlen > 0) {
        if (!binary) throw std::runtime_error("inet_nsap_ntoa: null binary");
        bytes.resize(static_cast<std::size_t>(binlen));
        if (!GuestCopyIn_nid_no_patch(bytes.data(), binary, bytes.size())) {
            *__error_nid_postfix() = GuestEfault;
            return nullptr;
        }
    }
    std::string text = "0x";
    for (int i = 0; i < binlen; i++) {
        int nibble = bytes[static_cast<std::size_t>(i)] >> 4;
        text += static_cast<char>(nibble + (nibble < 10 ? '0' : '7'));
        nibble = bytes[static_cast<std::size_t>(i)] & 0x0f;
        text += static_cast<char>(nibble + (nibble < 10 ? '0' : '7'));
        if ((i % 2) == 0 && (i + 1) < binlen) text += '.';
    }
    if (ascii) {
        if (!GuestCopyOut_nid_no_patch(ascii, text.c_str(), text.size() + 1)) {
            *__error_nid_postfix() = GuestEfault;
            return nullptr;
        }
    } else {
        std::memcpy(tmpbuf, text.c_str(), text.size() + 1);
    }
    return start;
}
