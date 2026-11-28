/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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
/*
 * Copyright (C) 2004, 2005, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1996, 1998, 1999, 2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/inet/inet_net_ntop.c and lib/libc/inet/inet_net_pton.c: https://github.com/freebsd/freebsd-src/tree/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int GuestAfInet = 2;
constexpr int GuestAfInet6 = 28;
constexpr int GuestEfault = 14;
constexpr int GuestEnoent = 2;
constexpr int GuestEinval = 22;
constexpr int GuestEmsgsize = 40;
constexpr int GuestEafnosupport = 47;

bool Fail(int error) {
    *__error_nid_postfix() = error;
    return false;
}

bool IsAscii(int value) { return value >= 0 && value < 128; }
bool IsDigit(int value) { return value >= '0' && value <= '9'; }
bool IsUpper(int value) { return value >= 'A' && value <= 'Z'; }
int ToLower(int value) { return IsUpper(value) ? value + ('a' - 'A') : value; }

int HexValue(int value) {
    if (IsDigit(value)) return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

class GuestText {
public:
    explicit GuestText(const char* text) : text_(text) {}

    int At(std::size_t index) {
        if (!ok_) return 0;
        char value = 0;
        if (!GuestCopyIn_nid_no_patch(&value, text_ + index, 1)) {
            ok_ = false;
            return 0;
        }
        return value;
    }

    bool Ok() const { return ok_; }

private:
    const char* text_;
    bool ok_ = true;
};

bool Ipv4Text(const std::uint8_t* source, int bits, std::size_t size, std::string& text) {
    if (bits < 0 || bits > 32) return Fail(GuestEinval);
    if (bits == 0) {
        if (size < sizeof "0") return Fail(GuestEmsgsize);
        text += '0';
        size--;
    }
    std::size_t index = 0;
    for (int whole = bits / 8; whole > 0; whole--) {
        if (size <= sizeof "255.") return Fail(GuestEmsgsize);
        const std::size_t start = text.size();
        text += std::to_string(source[index++]);
        if (whole > 1) text += '.';
        size -= text.size() - start;
    }
    const int partial = bits % 8;
    if (partial > 0) {
        if (size <= sizeof ".255") return Fail(GuestEmsgsize);
        const std::size_t start = text.size();
        if (!text.empty()) text += '.';
        const unsigned mask = ((1u << partial) - 1) << (8 - partial);
        text += std::to_string(source[index] & mask);
        size -= text.size() - start;
    }
    if (size <= sizeof "/32") return Fail(GuestEmsgsize);
    text += "/" + std::to_string(bits);
    return true;
}

std::string Hex(unsigned value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%x", value);
    return buffer;
}

bool Ipv6Text(const std::uint8_t* source, int bits, std::size_t size, std::string& text) {
    if (bits == 0) {
        text = "::";
    } else {
        const int bytes = (bits + 7) / 8;
        std::uint8_t in[16] = {};
        std::memcpy(in, source, static_cast<std::size_t>(bytes));
        const int partial = bits % 8;
        if (partial != 0) in[bytes - 1] &= static_cast<std::uint8_t>(~0u << (8 - partial));
        int words = (bits + 15) / 16;
        if (words == 1) words = 2;
        int zeroStart = 0;
        int zeroLength = 0;
        int tmpZeroStart = 0;
        int tmpZeroLength = 0;
        for (int i = 0; i < words * 2; i += 2) {
            if ((in[i] | in[i + 1]) == 0) {
                if (tmpZeroLength == 0) tmpZeroStart = i / 2;
                tmpZeroLength++;
            } else if (tmpZeroLength != 0 && zeroLength < tmpZeroLength) {
                zeroStart = tmpZeroStart;
                zeroLength = tmpZeroLength;
                tmpZeroLength = 0;
            }
        }
        if (tmpZeroLength != 0 && zeroLength < tmpZeroLength) {
            zeroStart = tmpZeroStart;
            zeroLength = tmpZeroLength;
        }
        const bool isIpv4 = zeroLength != words && zeroStart == 0 &&
            ((zeroLength == 6) || (zeroLength == 5 && in[10] == 0xff && in[11] == 0xff) ||
             (zeroLength == 7 && in[14] != 0 && in[15] != 1));
        std::size_t index = 0;
        for (int word = 0; word < words; word++) {
            if (zeroLength != 0 && word >= zeroStart && word < zeroStart + zeroLength) {
                if (word == zeroStart) text += ':';
                if (word == words - 1) text += ':';
                index += 2;
                continue;
            }
            if (isIpv4 && word > 5) {
                text += (word == 6) ? ':' : '.';
                text += std::to_string(in[index++]);
                if (word != 7 || bits > 120) {
                    text += '.';
                    text += std::to_string(in[index++]);
                }
            } else {
                if (!text.empty()) text += ':';
                text += Hex(static_cast<unsigned>(in[index]) * 256u + in[index + 1]);
                index += 2;
            }
        }
    }
    text += "/" + std::to_string(bits);
    if (text.size() + 1 > size) return Fail(GuestEmsgsize);
    return true;
}

bool GetBits(GuestText& source, std::size_t pos, int& bits) {
    int value = 0;
    int digits = 0;
    int ch;
    while ((ch = source.At(pos++)) != 0) {
        if (!IsDigit(ch)) return false;
        if (digits++ != 0 && value == 0) return false;
        value = value * 10 + (ch - '0');
        if (value > 128) return false;
    }
    if (digits == 0) return false;
    bits = value;
    return true;
}

bool GetV4(GuestText& source, std::size_t pos, std::uint8_t* out, int& bits) {
    std::size_t written = 0;
    unsigned value = 0;
    int digits = 0;
    int ch;
    while ((ch = source.At(pos++)) != 0) {
        if (IsDigit(ch)) {
            if (digits++ != 0 && value == 0) return false;
            value = value * 10 + static_cast<unsigned>(ch - '0');
            if (value > 255) return false;
            continue;
        }
        if (ch == '.' || ch == '/') {
            if (written > 3) return false;
            out[written++] = static_cast<std::uint8_t>(value);
            if (ch == '/') return GetBits(source, pos, bits);
            value = 0;
            digits = 0;
            continue;
        }
        return false;
    }
    if (digits == 0) return false;
    if (written > 3) return false;
    out[written++] = static_cast<std::uint8_t>(value);
    return true;
}

bool Ipv4Pton(GuestText& source, std::vector<std::uint8_t>& dst, std::size_t size, int& bits) {
    std::size_t pos = 0;
    int ch = source.At(pos++);
    if (ch == '0' && (source.At(pos) == 'x' || source.At(pos) == 'X') && IsAscii(source.At(pos + 1)) && HexValue(source.At(pos + 1)) >= 0) {
        if (size == 0) return Fail(GuestEmsgsize);
        int dirty = 0;
        unsigned tmp = 0;
        pos++;
        while ((ch = source.At(pos++)) != 0 && IsAscii(ch) && HexValue(ch) >= 0) {
            const int nibble = HexValue(ToLower(ch));
            tmp = dirty == 0 ? static_cast<unsigned>(nibble) : (tmp << 4) | static_cast<unsigned>(nibble);
            if (++dirty == 2) {
                if (size == 0) return Fail(GuestEmsgsize);
                size--;
                dst.push_back(static_cast<std::uint8_t>(tmp));
                dirty = 0;
            }
        }
        if (dirty) {
            if (size == 0) return Fail(GuestEmsgsize);
            size--;
            dst.push_back(static_cast<std::uint8_t>(tmp << 4));
        }
    } else if (IsAscii(ch) && IsDigit(ch)) {
        for (;;) {
            int tmp = 0;
            do {
                tmp = tmp * 10 + (ch - '0');
                if (tmp > 255) return Fail(GuestEnoent);
            } while ((ch = source.At(pos++)) != 0 && IsAscii(ch) && IsDigit(ch));
            if (size == 0) return Fail(GuestEmsgsize);
            size--;
            dst.push_back(static_cast<std::uint8_t>(tmp));
            if (ch == 0 || ch == '/') break;
            if (ch != '.') return Fail(GuestEnoent);
            ch = source.At(pos++);
            if (!IsAscii(ch) || !IsDigit(ch)) return Fail(GuestEnoent);
        }
    } else {
        return Fail(GuestEnoent);
    }
    bits = -1;
    if (ch == '/' && IsAscii(source.At(pos)) && IsDigit(source.At(pos)) && !dst.empty()) {
        ch = source.At(pos++);
        bits = 0;
        do {
            bits = bits * 10 + (ch - '0');
            if (bits > 32) return Fail(GuestEnoent);
        } while ((ch = source.At(pos++)) != 0 && IsAscii(ch) && IsDigit(ch));
        if (ch != 0) return Fail(GuestEnoent);
    }
    if (ch != 0) return Fail(GuestEnoent);
    if (dst.empty()) return Fail(GuestEnoent);
    if (bits == -1) {
        const std::uint8_t first = dst[0];
        if (first >= 240) {
            bits = 32;
        } else if (first >= 224) {
            bits = 8;
        } else if (first >= 192) {
            bits = 24;
        } else if (first >= 128) {
            bits = 16;
        } else {
            bits = 8;
        }
        if (bits < static_cast<int>(dst.size() * 8)) bits = static_cast<int>(dst.size() * 8);
        if (bits == 8 && first == 224) bits = 4;
    }
    while (bits > static_cast<int>(dst.size() * 8)) {
        if (size == 0) return Fail(GuestEmsgsize);
        size--;
        dst.push_back(0);
    }
    return true;
}

bool Ipv6Pton(GuestText& source, std::vector<std::uint8_t>& dst, std::size_t size, int& bits) {
    std::uint8_t tmp[16] = {};
    std::size_t tp = 0;
    std::size_t endp = 16;
    bool hasColon = false;
    std::size_t colonp = 0;
    std::size_t pos = 0;
    if (source.At(pos) == ':') {
        ++pos;
        if (source.At(pos) != ':') return Fail(GuestEnoent);
    }
    std::size_t curtok = pos;
    bool sawXdigit = false;
    unsigned val = 0;
    int digits = 0;
    bits = -1;
    bool ipv4 = false;
    int ch;
    while ((ch = source.At(pos++)) != 0) {
        const int nibble = HexValue(ch);
        if (nibble >= 0) {
            val = (val << 4) | static_cast<unsigned>(nibble);
            if (++digits > 4) return Fail(GuestEnoent);
            sawXdigit = true;
            continue;
        }
        if (ch == ':') {
            curtok = pos;
            if (!sawXdigit) {
                if (hasColon) return Fail(GuestEnoent);
                hasColon = true;
                colonp = tp;
                continue;
            }
            if (source.At(pos) == 0) return Fail(GuestEnoent);
            if (tp + 2 > endp) {
                bits = 0;
                return true;
            }
            tmp[tp++] = static_cast<std::uint8_t>((val >> 8) & 0xff);
            tmp[tp++] = static_cast<std::uint8_t>(val & 0xff);
            sawXdigit = false;
            digits = 0;
            val = 0;
            continue;
        }
        if (ch == '.' && tp + 4 <= endp && GetV4(source, curtok, tmp + tp, bits)) {
            tp += 4;
            sawXdigit = false;
            ipv4 = true;
            break;
        }
        if (ch == '/' && GetBits(source, pos, bits)) break;
        return Fail(GuestEnoent);
    }
    if (sawXdigit) {
        if (tp + 2 > endp) return Fail(GuestEnoent);
        tmp[tp++] = static_cast<std::uint8_t>((val >> 8) & 0xff);
        tmp[tp++] = static_cast<std::uint8_t>(val & 0xff);
    }
    if (bits == -1) bits = 128;
    int words = (bits + 15) / 16;
    if (words < 2) words = 2;
    if (ipv4) words = 8;
    endp = static_cast<std::size_t>(words) * 2;
    if (hasColon) {
        const std::size_t count = tp - colonp;
        if (tp == endp) return Fail(GuestEnoent);
        for (std::size_t i = 1; i <= count; i++) {
            if (endp < i) return Fail(GuestEnoent);
            tmp[endp - i] = tmp[colonp + count - i];
            tmp[colonp + count - i] = 0;
        }
        tp = endp;
    }
    if (tp != endp) return Fail(GuestEnoent);
    const std::size_t bytes = static_cast<std::size_t>(bits + 7) / 8;
    if (bytes > size) return Fail(GuestEmsgsize);
    dst.assign(tmp, tmp + bytes);
    return true;
}

}

extern "C" char* APS5_VABI inet_net_ntop_nid_postfix(int af, const void* source, int bits, char* destination, std::size_t size) {
    if (af != GuestAfInet && af != GuestAfInet6) {
        *__error_nid_postfix() = GuestEafnosupport;
        return nullptr;
    }
    const int maximum = af == GuestAfInet ? 32 : 128;
    if (bits < 0 || bits > maximum) {
        *__error_nid_postfix() = GuestEinval;
        return nullptr;
    }
    std::uint8_t address[16] = {};
    const int bytes = (bits + 7) / 8;
    if (bytes > 0) {
        if (!source) throw std::runtime_error("inet_net_ntop: null source");
        if (!GuestCopyIn_nid_no_patch(address, source, static_cast<std::size_t>(bytes))) {
            *__error_nid_postfix() = GuestEfault;
            return nullptr;
        }
    }
    if (!destination) throw std::runtime_error("inet_net_ntop: null destination");
    std::string text;
    const bool converted = af == GuestAfInet ? Ipv4Text(address, bits, size, text) : Ipv6Text(address, bits, size, text);
    if (!converted) return nullptr;
    if (!GuestCopyOut_nid_no_patch(destination, text.c_str(), text.size() + 1)) {
        *__error_nid_postfix() = GuestEfault;
        return nullptr;
    }
    return destination;
}

extern "C" int APS5_VABI inet_net_pton_nid_postfix(int af, const char* source, void* destination, std::size_t size) {
    if (af != GuestAfInet && af != GuestAfInet6) {
        *__error_nid_postfix() = GuestEafnosupport;
        return -1;
    }
    if (!source) throw std::runtime_error("inet_net_pton: null source");
    if (!destination) throw std::runtime_error("inet_net_pton: null destination");
    GuestText text(source);
    std::vector<std::uint8_t> bytes;
    int bits = -1;
    const bool parsed = af == GuestAfInet ? Ipv4Pton(text, bytes, size, bits) : Ipv6Pton(text, bytes, size, bits);
    if (!text.Ok()) {
        *__error_nid_postfix() = GuestEfault;
        return -1;
    }
    if (!parsed) return -1;
    if (!bytes.empty() && !GuestCopyOut_nid_no_patch(destination, bytes.data(), bytes.size())) {
        *__error_nid_postfix() = GuestEfault;
        return -1;
    }
    return bits;
}
