#include <nid/Sha1.hpp>
#include <cstdint>
#include <array>
#include <vector>

namespace Nid {

namespace {

constexpr std::uint32_t _rotl32(std::uint32_t x, unsigned n) {
    return (x << n) | (x >> (32u - n));
}

}

std::array<std::uint8_t, 20> Sha1(const std::vector<std::uint8_t>& data) {
    std::uint32_t h0 = 0x67452301u;
    std::uint32_t h1 = 0xEFCDAB89u;
    std::uint32_t h2 = 0x98BADCFEu;
    std::uint32_t h3 = 0x10325476u;
    std::uint32_t h4 = 0xC3D2E1F0u;

    std::vector<std::uint8_t> msg(data);

    const std::uint64_t bitLen = static_cast<std::uint64_t>(data.size()) * 8u;

    msg.push_back(0x80u);
    while ((msg.size() % 64u) != 56u)
        msg.push_back(0x00u);

    for (int i = 7; i >= 0; --i)
        msg.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8u)) & 0xffu));

    for (std::size_t offset = 0; offset < msg.size(); offset += 64u) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(msg[offset + i * 4u + 0u]) << 24u)
                 | (static_cast<std::uint32_t>(msg[offset + i * 4u + 1u]) << 16u)
                 | (static_cast<std::uint32_t>(msg[offset + i * 4u + 2u]) <<  8u)
                 | (static_cast<std::uint32_t>(msg[offset + i * 4u + 3u]));
        }
        for (int i = 16; i < 80; ++i)
            w[i] = _rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1u);

        std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            std::uint32_t temp = _rotl32(a, 5u) + f + e + k + w[i];
            e = d; d = c; c = _rotl32(b, 30u); b = a; a = temp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<std::uint8_t, 20> digest;
    auto _put = [&](int base, std::uint32_t v) {
        digest[base+0] = static_cast<std::uint8_t>(v >> 24u);
        digest[base+1] = static_cast<std::uint8_t>(v >> 16u);
        digest[base+2] = static_cast<std::uint8_t>(v >>  8u);
        digest[base+3] = static_cast<std::uint8_t>(v);
    };
    _put(0, h0); _put(4, h1); _put(8, h2); _put(12, h3); _put(16, h4);
    return digest;
}

}
