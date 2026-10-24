#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

namespace {
struct Md4Context {
    std::uint32_t A, B, C, D;
    std::uint32_t Nl, Nh;
    std::uint32_t data[16];
    std::uint32_t num;
};

struct Md5Context {
    std::uint32_t A, B, C, D;
    std::uint32_t Nl, Nh;
    std::uint32_t data[16];
    std::uint32_t num;
};

struct Sha1Context {
    std::uint32_t h[5];
    std::uint32_t Nl, Nh;
    std::uint32_t data[16];
    std::uint32_t num;
};

struct Sha256Context {
    std::uint32_t h[8];
    std::uint32_t Nl, Nh;
    std::uint32_t data[16];
    std::uint32_t num, mdLength;
};

struct Sha512Context {
    std::uint64_t h[8];
    std::uint64_t Nl, Nh;
    std::uint64_t data[16];
    std::uint32_t num, mdLength;
};

static_assert(sizeof(Md4Context) == 92);
static_assert(sizeof(Md5Context) == 92);
static_assert(sizeof(Sha1Context) == 96);
static_assert(sizeof(Sha256Context) == 112);
static_assert(sizeof(Sha512Context) == 216);

constexpr std::size_t Sha224Length = 28;
constexpr std::size_t Sha256Length = 32;
constexpr std::size_t Sha384Length = 48;
constexpr std::size_t Sha512Length = 64;

std::uint32_t LoadLittle32(const unsigned char* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}

std::uint32_t LoadBig32(const unsigned char* p) {
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | std::uint32_t(p[3]);
}

std::uint64_t LoadBig64(const unsigned char* p) {
    return std::uint64_t(LoadBig32(p)) << 32 | LoadBig32(p + 4);
}

void StoreLittle32(unsigned char* p, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (8 * i));
}

void StoreBig32(unsigned char* p, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (24 - 8 * i));
}

void StoreBig64(unsigned char* p, std::uint64_t value) {
    StoreBig32(p, static_cast<std::uint32_t>(value >> 32));
    StoreBig32(p + 4, static_cast<std::uint32_t>(value));
}

void Md4Block(Md4Context& c, const unsigned char* block) {
    std::uint32_t x[16];
    for (int i = 0; i < 16; ++i) x[i] = LoadLittle32(block + 4 * i);
    std::uint32_t a = c.A, b = c.B, cc = c.C, d = c.D;
    const auto f = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return (x1 & y) | (~x1 & z); };
    const auto g = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return (x1 & y) | (x1 & z) | (y & z); };
    const auto h = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return x1 ^ y ^ z; };
    static constexpr int shifts1[4] {3, 7, 11, 19};
    static constexpr int shifts2[4] {3, 5, 9, 13};
    static constexpr int shifts3[4] {3, 9, 11, 15};
    static constexpr int order2[16] {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    static constexpr int order3[16] {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    for (int i = 0; i < 16; ++i) {
        const auto t = std::rotl(a + f(b, cc, d) + x[i], shifts1[i % 4]);
        a = d; d = cc; cc = b; b = t;
    }
    for (int i = 0; i < 16; ++i) {
        const auto t = std::rotl(a + g(b, cc, d) + x[order2[i]] + 0x5a827999u, shifts2[i % 4]);
        a = d; d = cc; cc = b; b = t;
    }
    for (int i = 0; i < 16; ++i) {
        const auto t = std::rotl(a + h(b, cc, d) + x[order3[i]] + 0x6ed9eba1u, shifts3[i % 4]);
        a = d; d = cc; cc = b; b = t;
    }
    c.A += a; c.B += b; c.C += cc; c.D += d;
}

void Md5Block(Md5Context& c, const unsigned char* block) {
    static constexpr std::uint32_t constants[64] {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static constexpr int shifts[16] {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
    std::uint32_t x[16];
    for (int i = 0; i < 16; ++i) x[i] = LoadLittle32(block + 4 * i);
    std::uint32_t a = c.A, b = c.B, cc = c.C, d = c.D;
    for (int i = 0; i < 64; ++i) {
        std::uint32_t mix;
        int index;
        switch (i / 16) {
        case 0: mix = (b & cc) | (~b & d); index = i; break;
        case 1: mix = (d & b) | (~d & cc); index = (5 * i + 1) % 16; break;
        case 2: mix = b ^ cc ^ d; index = (3 * i + 5) % 16; break;
        default: mix = cc ^ (b | ~d); index = (7 * i) % 16; break;
        }
        const auto t = d;
        d = cc;
        cc = b;
        b += std::rotl(a + mix + constants[i] + x[index], shifts[(i / 16) * 4 + i % 4]);
        a = t;
    }
    c.A += a; c.B += b; c.C += cc; c.D += d;
}

void Sha1Block(Sha1Context& c, const unsigned char* block) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = LoadBig32(block + 4 * i);
    for (int i = 16; i < 80; ++i) w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3], e = c.h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20) { f = (b & cc) | (~b & d); k = 0x5a827999; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8f1bbcdc; }
        else { f = b ^ cc ^ d; k = 0xca62c1d6; }
        const auto t = std::rotl(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = std::rotl(b, 30); b = a; a = t;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d; c.h[4] += e;
}

void Sha256Block(Sha256Context& c, const unsigned char* block) {
    static constexpr std::uint32_t constants[64] {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = LoadBig32(block + 4 * i);
    for (int i = 16; i < 64; ++i) {
        const auto s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const auto s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t v[8];
    std::memcpy(v, c.h, sizeof(v));
    for (int i = 0; i < 64; ++i) {
        const auto s1 = std::rotr(v[4], 6) ^ std::rotr(v[4], 11) ^ std::rotr(v[4], 25);
        const auto choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
        const auto t1 = v[7] + s1 + choice + constants[i] + w[i];
        const auto s0 = std::rotr(v[0], 2) ^ std::rotr(v[0], 13) ^ std::rotr(v[0], 22);
        const auto majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        std::memmove(v + 1, v, 7 * sizeof(v[0]));
        v[4] += t1;
        v[0] = t1 + s0 + majority;
    }
    for (int i = 0; i < 8; ++i) c.h[i] += v[i];
}

void Sha512Block(Sha512Context& c, const unsigned char* block) {
    static constexpr std::uint64_t constants[80] {
        0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc, 0x3956c25bf348b538,
        0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242, 0x12835b0145706fbe,
        0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
        0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
        0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5, 0x983e5152ee66dfab,
        0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
        0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed,
        0x53380d139d95b3df, 0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
        0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
        0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8, 0x19a4c116b8d2d0c8, 0x1e376c085141ab53,
        0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373,
        0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
        0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b, 0xca273eceea26619c,
        0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba, 0x0a637dc5a2c898a6,
        0x113f9804bef90dae, 0x1b710b35131c471b, 0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
        0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
    };
    std::uint64_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = LoadBig64(block + 8 * i);
    for (int i = 16; i < 80; ++i) {
        const auto s0 = std::rotr(w[i - 15], 1) ^ std::rotr(w[i - 15], 8) ^ (w[i - 15] >> 7);
        const auto s1 = std::rotr(w[i - 2], 19) ^ std::rotr(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint64_t v[8];
    std::memcpy(v, c.h, sizeof(v));
    for (int i = 0; i < 80; ++i) {
        const auto s1 = std::rotr(v[4], 14) ^ std::rotr(v[4], 18) ^ std::rotr(v[4], 41);
        const auto choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
        const auto t1 = v[7] + s1 + choice + constants[i] + w[i];
        const auto s0 = std::rotr(v[0], 28) ^ std::rotr(v[0], 34) ^ std::rotr(v[0], 39);
        const auto majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        std::memmove(v + 1, v, 7 * sizeof(v[0]));
        v[4] += t1;
        v[0] = t1 + s0 + majority;
    }
    for (int i = 0; i < 8; ++i) c.h[i] += v[i];
}

template<class TContext> TContext& RequireContext(TContext* context, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    return *context;
}

void RequireInput(const void* data, std::size_t length, const char* function) {
    if (!data && length) throw std::invalid_argument(std::string(function) + ": null input");
}

void RequireDigest(const unsigned char* digest, const char* function) {
    if (!digest) throw std::invalid_argument(std::string(function) + ": null digest buffer");
}

void RequireBuffered(std::uint32_t buffered, std::size_t block, const char* function) {
    if (buffered >= block) throw std::invalid_argument(std::string(function) + ": buffered byte count is not below the block size");
}

template<class TContext, class TBlock> void Update32(TContext& c, const void* input, std::size_t length, TBlock block) {
    constexpr std::size_t blockSize = 64;
    auto* buffer = reinterpret_cast<unsigned char*>(c.data);
    auto* data = static_cast<const unsigned char*>(input);
    const std::uint64_t bits = (std::uint64_t(c.Nh) << 32 | c.Nl) + (std::uint64_t(length) << 3);
    c.Nl = static_cast<std::uint32_t>(bits);
    c.Nh = static_cast<std::uint32_t>(bits >> 32);
    if (c.num) {
        const std::size_t room = blockSize - c.num;
        if (length < room) {
            std::memcpy(buffer + c.num, data, length);
            c.num += static_cast<std::uint32_t>(length);
            return;
        }
        std::memcpy(buffer + c.num, data, room);
        block(c, buffer);
        data += room;
        length -= room;
        c.num = 0;
        std::memset(buffer, 0, blockSize);
    }
    for (; length >= blockSize; data += blockSize, length -= blockSize) block(c, data);
    if (length) {
        std::memcpy(buffer, data, length);
        c.num = static_cast<std::uint32_t>(length);
    }
}

template<class TContext, class TBlock> void Final32(TContext& c, bool bigEndian, TBlock block) {
    constexpr std::size_t blockSize = 64;
    auto* buffer = reinterpret_cast<unsigned char*>(c.data);
    std::size_t n = c.num;
    buffer[n++] = 0x80;
    if (n > blockSize - 8) {
        std::memset(buffer + n, 0, blockSize - n);
        n = 0;
        block(c, buffer);
    }
    std::memset(buffer + n, 0, blockSize - 8 - n);
    if (bigEndian) {
        StoreBig32(buffer + 56, c.Nh);
        StoreBig32(buffer + 60, c.Nl);
    } else {
        StoreLittle32(buffer + 56, c.Nl);
        StoreLittle32(buffer + 60, c.Nh);
    }
    block(c, buffer);
    c.num = 0;
    std::memset(buffer, 0, blockSize);
}

void Sha512Update(Sha512Context& c, const void* input, std::size_t length) {
    constexpr std::size_t blockSize = 128;
    auto* buffer = reinterpret_cast<unsigned char*>(c.data);
    auto* data = static_cast<const unsigned char*>(input);
    const std::uint64_t low = c.Nl + (std::uint64_t(length) << 3);
    if (low < c.Nl) ++c.Nh;
    c.Nh += std::uint64_t(length) >> 61;
    c.Nl = low;
    if (c.num) {
        const std::size_t room = blockSize - c.num;
        if (length < room) {
            std::memcpy(buffer + c.num, data, length);
            c.num += static_cast<std::uint32_t>(length);
            return;
        }
        std::memcpy(buffer + c.num, data, room);
        Sha512Block(c, buffer);
        data += room;
        length -= room;
        c.num = 0;
    }
    for (; length >= blockSize; data += blockSize, length -= blockSize) Sha512Block(c, data);
    if (length) {
        std::memcpy(buffer, data, length);
        c.num = static_cast<std::uint32_t>(length);
    }
}

int Sha512Final(unsigned char* digest, Sha512Context& c) {
    constexpr std::size_t blockSize = 128;
    auto* buffer = reinterpret_cast<unsigned char*>(c.data);
    std::size_t n = c.num;
    buffer[n++] = 0x80;
    if (n > blockSize - 16) {
        std::memset(buffer + n, 0, blockSize - n);
        n = 0;
        Sha512Block(c, buffer);
    }
    std::memset(buffer + n, 0, blockSize - 16 - n);
    StoreBig64(buffer + 112, c.Nh);
    StoreBig64(buffer + 120, c.Nl);
    Sha512Block(c, buffer);
    if (!digest) return 0;
    if (c.mdLength == Sha224Length || c.mdLength == Sha256Length)
        NotImplemented_nid_no_patch("SHA512_Final: SHA-512/224 and SHA-512/256 digest lengths");
    if (c.mdLength != Sha384Length && c.mdLength != Sha512Length) return 0;
    for (std::size_t i = 0; i < c.mdLength / 8; ++i) StoreBig64(digest + 8 * i, c.h[i]);
    return 1;
}

int Sha256Final(unsigned char* digest, Sha256Context& c, const char* function) {
    RequireDigest(digest, function);
    RequireBuffered(c.num, 64, function);
    Final32(c, true, Sha256Block);
    if (c.mdLength > Sha256Length) return 0;
    for (std::size_t i = 0; i < c.mdLength / 4; ++i) StoreBig32(digest + 4 * i, c.h[i]);
    return 1;
}
}

extern "C" {

int APS5_VABI MD4_Init_nid_postfix(Md4Context* context) {
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    c.A = 0x67452301;
    c.B = 0xefcdab89;
    c.C = 0x98badcfe;
    c.D = 0x10325476;
    return 1;
}

int APS5_VABI MD4_Update_nid_postfix(Md4Context* context, const void* data, std::size_t length) {
    auto& c = RequireContext(context, __func__);
    if (!length) return 1;
    RequireInput(data, length, __func__);
    RequireBuffered(c.num, 64, __func__);
    Update32(c, data, length, Md4Block);
    return 1;
}

int APS5_VABI MD4_Final_nid_postfix(unsigned char* digest, Md4Context* context) {
    auto& c = RequireContext(context, __func__);
    RequireDigest(digest, __func__);
    RequireBuffered(c.num, 64, __func__);
    Final32(c, false, Md4Block);
    StoreLittle32(digest, c.A);
    StoreLittle32(digest + 4, c.B);
    StoreLittle32(digest + 8, c.C);
    StoreLittle32(digest + 12, c.D);
    return 1;
}

int APS5_VABI MD5_Init_nid_postfix(Md5Context* context) {
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    c.A = 0x67452301;
    c.B = 0xefcdab89;
    c.C = 0x98badcfe;
    c.D = 0x10325476;
    return 1;
}

int APS5_VABI MD5_Update_nid_postfix(Md5Context* context, const void* data, std::size_t length) {
    auto& c = RequireContext(context, __func__);
    if (!length) return 1;
    RequireInput(data, length, __func__);
    RequireBuffered(c.num, 64, __func__);
    Update32(c, data, length, Md5Block);
    return 1;
}

int APS5_VABI MD5_Final_nid_postfix(unsigned char* digest, Md5Context* context) {
    auto& c = RequireContext(context, __func__);
    RequireDigest(digest, __func__);
    RequireBuffered(c.num, 64, __func__);
    Final32(c, false, Md5Block);
    StoreLittle32(digest, c.A);
    StoreLittle32(digest + 4, c.B);
    StoreLittle32(digest + 8, c.C);
    StoreLittle32(digest + 12, c.D);
    return 1;
}

int APS5_VABI SHA1_Init_nid_postfix(Sha1Context* context) {
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    c.h[0] = 0x67452301;
    c.h[1] = 0xefcdab89;
    c.h[2] = 0x98badcfe;
    c.h[3] = 0x10325476;
    c.h[4] = 0xc3d2e1f0;
    return 1;
}

int APS5_VABI SHA1_Update_nid_postfix(Sha1Context* context, const void* data, std::size_t length) {
    auto& c = RequireContext(context, __func__);
    if (!length) return 1;
    RequireInput(data, length, __func__);
    RequireBuffered(c.num, 64, __func__);
    Update32(c, data, length, Sha1Block);
    return 1;
}

int APS5_VABI SHA1_Final_nid_postfix(unsigned char* digest, Sha1Context* context) {
    auto& c = RequireContext(context, __func__);
    RequireDigest(digest, __func__);
    RequireBuffered(c.num, 64, __func__);
    Final32(c, true, Sha1Block);
    for (int i = 0; i < 5; ++i) StoreBig32(digest + 4 * i, c.h[i]);
    return 1;
}

int APS5_VABI SHA224_Init_nid_postfix(Sha256Context* context) {
    static constexpr std::uint32_t initial[8] {0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4};
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    std::memcpy(c.h, initial, sizeof(initial));
    c.mdLength = Sha224Length;
    return 1;
}

int APS5_VABI SHA256_Init_nid_postfix(Sha256Context* context) {
    static constexpr std::uint32_t initial[8] {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    std::memcpy(c.h, initial, sizeof(initial));
    c.mdLength = Sha256Length;
    return 1;
}

int APS5_VABI SHA256_Update_nid_postfix(Sha256Context* context, const void* data, std::size_t length) {
    auto& c = RequireContext(context, __func__);
    if (!length) return 1;
    RequireInput(data, length, __func__);
    RequireBuffered(c.num, 64, __func__);
    Update32(c, data, length, Sha256Block);
    return 1;
}

int APS5_VABI SHA224_Update_nid_postfix(Sha256Context* context, const void* data, std::size_t length) {
    return SHA256_Update_nid_postfix(context, data, length);
}

int APS5_VABI SHA256_Final_nid_postfix(unsigned char* digest, Sha256Context* context) {
    return Sha256Final(digest, RequireContext(context, __func__), __func__);
}

int APS5_VABI SHA224_Final_nid_postfix(unsigned char* digest, Sha256Context* context) {
    return Sha256Final(digest, RequireContext(context, __func__), __func__);
}

unsigned char* APS5_VABI SHA256_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest) {
    RequireInput(data, length, __func__);
    if (!digest) NotImplemented_nid_no_patch("SHA256: null digest buffer");
    Sha256Context c;
    SHA256_Init_nid_postfix(&c);
    if (length) Update32(c, data, length, Sha256Block);
    Sha256Final(digest, c, __func__);
    return digest;
}

int APS5_VABI SHA384_Init_nid_postfix(Sha512Context* context) {
    static constexpr std::uint64_t initial[8] {
        0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939,
        0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4,
    };
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    std::memcpy(c.h, initial, sizeof(initial));
    c.mdLength = Sha384Length;
    return 1;
}

int APS5_VABI SHA512_Init_nid_postfix(Sha512Context* context) {
    static constexpr std::uint64_t initial[8] {
        0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
        0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
    };
    auto& c = RequireContext(context, __func__);
    std::memset(&c, 0, sizeof(c));
    std::memcpy(c.h, initial, sizeof(initial));
    c.mdLength = Sha512Length;
    return 1;
}

int APS5_VABI SHA512_Update_nid_postfix(Sha512Context* context, const void* data, std::size_t length) {
    auto& c = RequireContext(context, __func__);
    if (!length) return 1;
    RequireInput(data, length, __func__);
    RequireBuffered(c.num, 128, __func__);
    Sha512Update(c, data, length);
    return 1;
}

int APS5_VABI SHA384_Update_nid_postfix(Sha512Context* context, const void* data, std::size_t length) {
    return SHA512_Update_nid_postfix(context, data, length);
}

int APS5_VABI SHA512_Final_nid_postfix(unsigned char* digest, Sha512Context* context) {
    auto& c = RequireContext(context, __func__);
    RequireBuffered(c.num, 128, __func__);
    return Sha512Final(digest, c);
}

int APS5_VABI SHA384_Final_nid_postfix(unsigned char* digest, Sha512Context* context) {
    return SHA512_Final_nid_postfix(digest, context);
}

}
