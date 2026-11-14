// Derived from Source/WTF/wtf/SHA1.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::SHA1;

constexpr std::size_t BlockSize = 64;
constexpr std::size_t LengthOffset = 56;

std::uint32_t F(int t, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    if (t < 20) return (b & c) | ((~b) & d);
    if (t < 40) return b ^ c ^ d;
    if (t < 60) return (b & c) | (b & d) | (c & d);
    return b ^ c ^ d;
}

std::uint32_t K(int t) {
    if (t < 20) return 0x5a827999;
    if (t < 40) return 0x6ed9eba1;
    if (t < 60) return 0x8f1bbcdc;
    return 0xca62c1d6;
}

std::uint32_t RotateLeft(int n, std::uint32_t x) {
    return (x << n) | (x >> (32 - n));
}

void ProcessBlock(SHA1& sha1) {
    std::uint32_t w[80] = {0};
    for (int t = 0; t < 16; ++t) {
        const std::uint8_t* bytes = sha1.buffer + t * 4;
        w[t] = (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) | bytes[3];
    }
    for (int t = 16; t < 80; ++t)
        w[t] = RotateLeft(1, w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16]);

    std::uint32_t a = sha1.hash[0];
    std::uint32_t b = sha1.hash[1];
    std::uint32_t c = sha1.hash[2];
    std::uint32_t d = sha1.hash[3];
    std::uint32_t e = sha1.hash[4];

    for (int t = 0; t < 80; ++t) {
        std::uint32_t temp = RotateLeft(5, a) + F(t, b, c, d) + e + w[t] + K(t);
        e = d;
        d = c;
        c = RotateLeft(30, b);
        b = a;
        a = temp;
    }

    sha1.hash[0] += a;
    sha1.hash[1] += b;
    sha1.hash[2] += c;
    sha1.hash[3] += d;
    sha1.hash[4] += e;

    sha1.cursor = 0;
}

void Finalize(SHA1& sha1) {
    sha1.buffer[sha1.cursor++] = 0x80;
    if (sha1.cursor > LengthOffset) {
        while (sha1.cursor < BlockSize)
            sha1.buffer[sha1.cursor++] = 0x00;
        ProcessBlock(sha1);
    }

    for (std::size_t i = sha1.cursor; i < LengthOffset; ++i)
        sha1.buffer[i] = 0x00;

    std::uint64_t bits = sha1.totalBytes * 8;
    for (int i = 0; i < 8; ++i) {
        sha1.buffer[LengthOffset + (7 - i)] = bits & 0xFF;
        bits >>= 8;
    }
    sha1.cursor = BlockSize;
    ProcessBlock(sha1);
}

void Reset(SHA1& sha1) {
    sha1.cursor = 0;
    sha1.totalBytes = 0;
    sha1.hash[0] = 0x67452301;
    sha1.hash[1] = 0xefcdab89;
    sha1.hash[2] = 0x98badcfe;
    sha1.hash[3] = 0x10325476;
    sha1.hash[4] = 0xc3d2e1f0;
    std::memset(sha1.buffer, 0, sizeof(sha1.buffer));
}

}

extern "C" {

void APS5_VABI _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(SHA1* self, Jsc::WtfAbi::SHA1Digest* digest) {
    if (!self || !digest) throw std::invalid_argument("WTF::SHA1::computeHash requires an object and a digest");
    if (self->cursor >= BlockSize) throw std::invalid_argument("WTF::SHA1::computeHash: buffer cursor out of range");
    Finalize(*self);

    for (std::size_t i = 0; i < 5; ++i) {
        std::uint32_t hashValue = self->hash[i];
        for (int j = 0; j < 4; ++j) {
            (*digest)[4 * i + (3 - j)] = hashValue & 0xFF;
            hashValue >>= 8;
        }
    }

    Reset(*self);
}

}
