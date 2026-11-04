#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "WtfStringAbi.hpp"

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::SHA1;

constexpr std::size_t HashSize = 20;
constexpr std::size_t BlockSize = 64;
constexpr std::size_t LengthOffset = 56;

using Digest = std::array<std::uint8_t, HashSize>;

SHA1& Checked(SHA1* self) {
    if (!self) throw std::invalid_argument("WTF::SHA1: null object");
    return *self;
}

std::uint32_t RotateLeft(int n, std::uint32_t x) {
    return (x << n) | (x >> (32 - n));
}

std::uint32_t F(int t, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    if (t < 20) return (b & c) | (~b & d);
    if (t < 40) return b ^ c ^ d;
    if (t < 60) return (b & c) | (b & d) | (c & d);
    return b ^ c ^ d;
}

std::uint32_t K(int t) {
    if (t < 20) return 0x5A827999U;
    if (t < 40) return 0x6ED9EBA1U;
    if (t < 60) return 0x8F1BBCDCU;
    return 0xCA62C1D6U;
}

void Reset(SHA1& sha1) {
    sha1.cursor = 0;
    sha1.totalBytes = 0;
    sha1.hash[0] = 0x67452301U;
    sha1.hash[1] = 0xEFCDAB89U;
    sha1.hash[2] = 0x98BADCFEU;
    sha1.hash[3] = 0x10325476U;
    sha1.hash[4] = 0xC3D2E1F0U;
    std::memset(sha1.buffer, 0, sizeof(sha1.buffer));
}

void ProcessBlock(SHA1& sha1) {
    std::uint32_t w[80] = {};
    for (int t = 0; t < 16; ++t) {
        const auto* bytes = sha1.buffer + t * 4;
        w[t] = std::uint32_t{bytes[0]} << 24 | std::uint32_t{bytes[1]} << 16 | std::uint32_t{bytes[2]} << 8 | bytes[3];
    }
    for (int t = 16; t < 80; ++t) w[t] = RotateLeft(1, w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16]);
    auto a = sha1.hash[0];
    auto b = sha1.hash[1];
    auto c = sha1.hash[2];
    auto d = sha1.hash[3];
    auto e = sha1.hash[4];
    for (int t = 0; t < 80; ++t) {
        const auto temp = RotateLeft(5, a) + F(t, b, c, d) + e + w[t] + K(t);
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
        while (sha1.cursor < BlockSize) sha1.buffer[sha1.cursor++] = 0;
        ProcessBlock(sha1);
    }
    for (auto i = sha1.cursor; i < LengthOffset; ++i) sha1.buffer[i] = 0;
    auto bits = sha1.totalBytes * 8;
    for (int i = 0; i < 8; ++i) {
        sha1.buffer[LengthOffset + 7 - i] = static_cast<std::uint8_t>(bits);
        bits >>= 8;
    }
    sha1.cursor = BlockSize;
    ProcessBlock(sha1);
}

Digest ComputeHash(SHA1& sha1) {
    if (sha1.cursor >= BlockSize) throw std::invalid_argument("WTF::SHA1: buffer cursor out of range");
    Finalize(sha1);
    Digest digest;
    for (std::size_t i = 0; i < 5; ++i) {
        auto value = sha1.hash[i];
        for (int j = 0; j < 4; ++j) {
            digest[4 * i + 3 - j] = static_cast<std::uint8_t>(value);
            value >>= 8;
        }
    }
    Reset(sha1);
    return digest;
}

CString HexDigest(const Digest& digest) {
    static constexpr char Hex[] = "0123456789ABCDEF";
    char* output = nullptr;
    auto result = _ZN3WTF7CString16newUninitializedEmRPc(HashSize * 2, output);
    for (std::size_t i = 0; i < HashSize; ++i) {
        output[2 * i] = Hex[digest[i] >> 4];
        output[2 * i + 1] = Hex[digest[i] & 15];
    }
    return result;
}

}  // namespace

void APS5_VABI _ZN3WTF4SHA1C1Ev(SHA1* self) {
    Reset(Checked(self));
}

void APS5_VABI _ZN3WTF4SHA1C2Ev(SHA1* self) {
    Reset(Checked(self));
}

CString APS5_VABI _ZN3WTF4SHA116computeHexDigestEv(SHA1* self) {
    return HexDigest(ComputeHash(Checked(self)));
}
