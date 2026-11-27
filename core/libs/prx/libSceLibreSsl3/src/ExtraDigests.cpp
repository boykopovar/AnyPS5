#include "prx/libSceLibreSsl3/include/ExtraDigests.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <mbedtls/ripemd160.h>
#include <mbedtls/sha3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t PkeyRsa = 6;
constexpr std::int32_t PkeyRsa2 = 19;
constexpr std::uint64_t DigestAlgorithmIdAbsent = 0x8;
constexpr std::int32_t PointerSize = static_cast<std::int32_t>(sizeof(void*));

constexpr std::int32_t NidMd4 = 257;
constexpr std::int32_t NidMd4WithRsa = 396;
constexpr std::int32_t NidSm3 = 968;
constexpr std::int32_t NidSm3WithRsa = 969;
constexpr std::int32_t NidMd5Sha1 = 114;
constexpr std::int32_t NidRipemd160 = 117;
constexpr std::int32_t NidRipemd160WithRsa = 119;
constexpr std::int32_t NidSha512_224WithRsa = 1025;
constexpr std::int32_t NidSha512_224 = 1029;
constexpr std::int32_t NidSha3_224 = 1031;
constexpr std::int32_t NidSha3_256 = 1032;
constexpr std::int32_t NidSha3_384 = 1033;
constexpr std::int32_t NidSha3_512 = 1034;
constexpr std::int32_t NidRsaSha3_224 = 1049;
constexpr std::int32_t NidRsaSha3_256 = 1050;
constexpr std::int32_t NidRsaSha3_384 = 1051;
constexpr std::int32_t NidRsaSha3_512 = 1052;

constexpr std::int32_t Ripemd160ContextSize = 96;
constexpr std::int32_t Sha3ContextSize = 224;

struct Md5Sha1State {
    Md5Context md5;
    Sha1Context sha1;
};

struct Md4State {
    std::uint32_t a, b, c, d;
    std::uint32_t nl, nh;
    std::uint32_t data[16];
    std::uint32_t num;
};

struct Sm3State {
    std::uint32_t h[8];
    std::uint32_t nl, nh;
    std::uint32_t data[16];
    std::uint32_t num;
};

static_assert(sizeof(Md4State) == 92 && sizeof(Sm3State) == 108);
static_assert(sizeof(Md5Context) == 92 && sizeof(Sha1Context) == 96 && sizeof(Md5Sha1State) == 188);
static_assert(sizeof(Sha512Context) == 216);
static_assert(sizeof(mbedtls_ripemd160_context) <= Ripemd160ContextSize);
static_assert(sizeof(mbedtls_sha3_context) <= Sha3ContextSize);

template <typename TState>
TState& State(EvpMdContext* context, const char* digest) {
    if (!context || !context->mdData) throw std::invalid_argument(std::string(digest) + ": digest context without data");
    return *static_cast<TState*>(context->mdData);
}

void RequireOutput(const unsigned char* digest, const char* name) {
    if (!digest) throw std::invalid_argument(std::string(name) + ": null digest buffer");
}

void Clear(unsigned char* data, std::size_t size) {
    volatile unsigned char* cleared = data;
    for (std::size_t index = 0; index < size; ++index) cleared[index] = 0;
}

int APS5_VABI Sha512_224Init(EvpMdContext* context) {
    static constexpr std::uint64_t Initial[8] {
        0x8c3d37c819544da2, 0x73e1996689dcd4d6, 0x1dfab7ae32ff9c82, 0x679dd514582f9fcf,
        0x0f6d2b697bd44da8, 0x77e36f7304c48942, 0x3f9d85a86a1d36c8, 0x1112e6ad91d692a1,
    };
    auto& state = State<Sha512Context>(context, "EVP_sha512_224");
    if (SHA512_Init_nid_postfix(&state) != 1) return 0;
    std::memcpy(state.h, Initial, sizeof(Initial));
    return 1;
}

int APS5_VABI Sha512_224Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA512_Update_nid_postfix(&State<Sha512Context>(context, "EVP_sha512_224"), data, length);
}

int APS5_VABI Sha512_224Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, "EVP_sha512_224");
    std::array<unsigned char, 64> full{};
    const int result = SHA512_Final_nid_postfix(full.data(), &State<Sha512Context>(context, "EVP_sha512_224"));
    if (result == 1) std::memcpy(digest, full.data(), 28);
    Clear(full.data(), full.size());
    return result;
}

int APS5_VABI Ripemd160Init(EvpMdContext* context) {
    auto& state = State<mbedtls_ripemd160_context>(context, "EVP_ripemd160");
    mbedtls_ripemd160_init(&state);
    return mbedtls_ripemd160_starts(&state) == 0 ? 1 : 0;
}

int APS5_VABI Ripemd160Update(EvpMdContext* context, const void* data, std::size_t length) {
    auto& state = State<mbedtls_ripemd160_context>(context, "EVP_ripemd160");
    return mbedtls_ripemd160_update(&state, static_cast<const unsigned char*>(data), length) == 0 ? 1 : 0;
}

int APS5_VABI Ripemd160Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, "EVP_ripemd160");
    return mbedtls_ripemd160_finish(&State<mbedtls_ripemd160_context>(context, "EVP_ripemd160"), digest) == 0 ? 1 : 0;
}

int APS5_VABI Md5Sha1Init(EvpMdContext* context) {
    auto& state = State<Md5Sha1State>(context, "EVP_md5_sha1");
    if (MD5_Init_nid_postfix(&state.md5) != 1) return 0;
    return SHA1_Init_nid_postfix(&state.sha1) == 1 ? 1 : 0;
}

int APS5_VABI Md5Sha1Update(EvpMdContext* context, const void* data, std::size_t length) {
    auto& state = State<Md5Sha1State>(context, "EVP_md5_sha1");
    if (MD5_Update_nid_postfix(&state.md5, data, length) != 1) return 0;
    return SHA1_Update_nid_postfix(&state.sha1, data, length) == 1 ? 1 : 0;
}

int APS5_VABI Md5Sha1Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, "EVP_md5_sha1");
    auto& state = State<Md5Sha1State>(context, "EVP_md5_sha1");
    if (MD5_Final_nid_postfix(digest, &state.md5) != 1) return 0;
    return SHA1_Final_nid_postfix(digest + 16, &state.sha1) == 1 ? 1 : 0;
}

std::uint32_t LoadLittle(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
        static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
}

std::uint32_t LoadBig(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16 |
        static_cast<std::uint32_t>(bytes[2]) << 8 | static_cast<std::uint32_t>(bytes[3]);
}

void StoreLittle(unsigned char* bytes, std::uint32_t value) {
    for (int index = 0; index < 4; ++index) bytes[index] = static_cast<unsigned char>(value >> (8 * index));
}

void StoreBig(unsigned char* bytes, std::uint32_t value) {
    for (int index = 0; index < 4; ++index) bytes[index] = static_cast<unsigned char>(value >> (24 - 8 * index));
}

void Md4Block(Md4State& state, const unsigned char* block) {
    std::uint32_t x[16];
    for (int index = 0; index < 16; ++index) x[index] = LoadLittle(block + 4 * index);
    std::uint32_t a = state.a;
    std::uint32_t b = state.b;
    std::uint32_t c = state.c;
    std::uint32_t d = state.d;
    const auto f = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return (x1 & y) | (~x1 & z); };
    const auto g = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return (x1 & y) | (x1 & z) | (y & z); };
    const auto h = [](std::uint32_t x1, std::uint32_t y, std::uint32_t z) { return x1 ^ y ^ z; };
    static constexpr int FirstShifts[4] {3, 7, 11, 19};
    static constexpr int SecondShifts[4] {3, 5, 9, 13};
    static constexpr int ThirdShifts[4] {3, 9, 11, 15};
    static constexpr int SecondOrder[16] {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    static constexpr int ThirdOrder[16] {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    const auto rotate = [&](std::uint32_t mixed, int shift) {
        const std::uint32_t next = std::rotl(a + mixed, shift);
        a = d;
        d = c;
        c = b;
        b = next;
    };
    for (int step = 0; step < 16; ++step) rotate(f(b, c, d) + x[step], FirstShifts[step % 4]);
    for (int step = 0; step < 16; ++step) rotate(g(b, c, d) + x[SecondOrder[step]] + 0x5a827999u, SecondShifts[step % 4]);
    for (int step = 0; step < 16; ++step) rotate(h(b, c, d) + x[ThirdOrder[step]] + 0x6ed9eba1u, ThirdShifts[step % 4]);
    state.a += a;
    state.b += b;
    state.c += c;
    state.d += d;
}

void Sm3Block(Sm3State& state, const unsigned char* block) {
    std::uint32_t w[68];
    for (int index = 0; index < 16; ++index) w[index] = LoadBig(block + 4 * index);
    const auto p0 = [](std::uint32_t x) { return x ^ std::rotl(x, 9) ^ std::rotl(x, 17); };
    const auto p1 = [](std::uint32_t x) { return x ^ std::rotl(x, 15) ^ std::rotl(x, 23); };
    for (int index = 16; index < 68; ++index)
        w[index] = p1(w[index - 16] ^ w[index - 9] ^ std::rotl(w[index - 3], 15)) ^ std::rotl(w[index - 13], 7) ^ w[index - 6];
    std::uint32_t v[8];
    std::copy(std::begin(state.h), std::end(state.h), v);
    for (int index = 0; index < 64; ++index) {
        const bool early = index < 16;
        const std::uint32_t constant = early ? 0x79cc4519u : 0x7a879d8au;
        const std::uint32_t rotatedA = std::rotl(v[0], 12);
        const std::uint32_t ss1 = std::rotl(rotatedA + v[4] + std::rotl(constant, index % 32), 7);
        const std::uint32_t ss2 = ss1 ^ rotatedA;
        const std::uint32_t ff = early ? v[0] ^ v[1] ^ v[2] : (v[0] & v[1]) | (v[0] & v[2]) | (v[1] & v[2]);
        const std::uint32_t gg = early ? v[4] ^ v[5] ^ v[6] : (v[4] & v[5]) | (~v[4] & v[6]);
        const std::uint32_t tt1 = ff + v[3] + ss2 + (w[index] ^ w[index + 4]);
        const std::uint32_t tt2 = gg + v[7] + ss1 + w[index];
        v[3] = v[2];
        v[2] = std::rotl(v[1], 9);
        v[1] = v[0];
        v[0] = tt1;
        v[7] = v[6];
        v[6] = std::rotl(v[5], 19);
        v[5] = v[4];
        v[4] = p0(tt2);
    }
    for (int index = 0; index < 8; ++index) state.h[index] ^= v[index];
}

template <typename TState, void (*TBlock)(TState&, const unsigned char*)>
int Absorb(TState& state, const void* data, std::size_t length, const char* name) {
    if (length == 0) return 1;
    if (!data) throw std::invalid_argument(std::string(name) + ": null data");
    if (state.num >= 64) throw std::invalid_argument(std::string(name) + ": corrupt digest state");
    const std::uint64_t bits = ((static_cast<std::uint64_t>(state.nh) << 32) | state.nl) + (static_cast<std::uint64_t>(length) << 3);
    state.nl = static_cast<std::uint32_t>(bits);
    state.nh = static_cast<std::uint32_t>(bits >> 32);
    auto* buffer = reinterpret_cast<unsigned char*>(state.data);
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::size_t used = state.num;
    if (used != 0) {
        const std::size_t taken = std::min<std::size_t>(64 - used, length);
        std::memcpy(buffer + used, bytes, taken);
        used += taken;
        bytes += taken;
        length -= taken;
        if (used < 64) {
            state.num = static_cast<std::uint32_t>(used);
            return 1;
        }
        TBlock(state, buffer);
    }
    for (; length >= 64; bytes += 64, length -= 64) TBlock(state, bytes);
    std::memcpy(buffer, bytes, length);
    state.num = static_cast<std::uint32_t>(length);
    return 1;
}

template <typename TState, void (*TBlock)(TState&, const unsigned char*)>
void Pad(TState& state, bool bigEndianLength, const char* name) {
    if (state.num >= 64) throw std::invalid_argument(std::string(name) + ": corrupt digest state");
    auto* buffer = reinterpret_cast<unsigned char*>(state.data);
    std::size_t used = state.num;
    buffer[used++] = 0x80;
    if (used > 56) {
        std::memset(buffer + used, 0, 64 - used);
        TBlock(state, buffer);
        used = 0;
    }
    std::memset(buffer + used, 0, 56 - used);
    if (bigEndianLength) {
        StoreBig(buffer + 56, state.nh);
        StoreBig(buffer + 60, state.nl);
    } else {
        StoreLittle(buffer + 56, state.nl);
        StoreLittle(buffer + 60, state.nh);
    }
    TBlock(state, buffer);
    state.num = 0;
    Clear(buffer, 64);
}

int APS5_VABI Md4Init(EvpMdContext* context) {
    auto& state = State<Md4State>(context, "EVP_md4");
    state = {};
    state.a = 0x67452301u;
    state.b = 0xefcdab89u;
    state.c = 0x98badcfeu;
    state.d = 0x10325476u;
    return 1;
}

int APS5_VABI Md4Update(EvpMdContext* context, const void* data, std::size_t length) {
    return Absorb<Md4State, Md4Block>(State<Md4State>(context, "EVP_md4"), data, length, "EVP_md4");
}

int APS5_VABI Md4Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, "EVP_md4");
    auto& state = State<Md4State>(context, "EVP_md4");
    Pad<Md4State, Md4Block>(state, false, "EVP_md4");
    StoreLittle(digest, state.a);
    StoreLittle(digest + 4, state.b);
    StoreLittle(digest + 8, state.c);
    StoreLittle(digest + 12, state.d);
    return 1;
}

int APS5_VABI Sm3Init(EvpMdContext* context) {
    auto& state = State<Sm3State>(context, "EVP_sm3");
    state = {{0x7380166fu, 0x4914b2b9u, 0x172442d7u, 0xda8a0600u, 0xa96f30bcu, 0x163138aau, 0xe38dee4du, 0xb0fb0e4eu}, 0, 0, {}, 0};
    return 1;
}

int APS5_VABI Sm3Update(EvpMdContext* context, const void* data, std::size_t length) {
    return Absorb<Sm3State, Sm3Block>(State<Sm3State>(context, "EVP_sm3"), data, length, "EVP_sm3");
}

int APS5_VABI Sm3Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, "EVP_sm3");
    auto& state = State<Sm3State>(context, "EVP_sm3");
    Pad<Sm3State, Sm3Block>(state, true, "EVP_sm3");
    for (int index = 0; index < 8; ++index) StoreBig(digest + 4 * index, state.h[index]);
    return 1;
}

template <mbedtls_sha3_id TId>
constexpr const char* Sha3Name() {
    if constexpr (TId == MBEDTLS_SHA3_224) return "EVP_sha3_224";
    else if constexpr (TId == MBEDTLS_SHA3_256) return "EVP_sha3_256";
    else if constexpr (TId == MBEDTLS_SHA3_384) return "EVP_sha3_384";
    else return "EVP_sha3_512";
}

template <mbedtls_sha3_id TId>
constexpr std::size_t Sha3Length() {
    if constexpr (TId == MBEDTLS_SHA3_224) return 28;
    else if constexpr (TId == MBEDTLS_SHA3_256) return 32;
    else if constexpr (TId == MBEDTLS_SHA3_384) return 48;
    else return 64;
}

template <mbedtls_sha3_id TId>
int APS5_VABI Sha3Init(EvpMdContext* context) {
    auto& state = State<mbedtls_sha3_context>(context, Sha3Name<TId>());
    mbedtls_sha3_init(&state);
    return mbedtls_sha3_starts(&state, TId) == 0 ? 1 : 0;
}

template <mbedtls_sha3_id TId>
int APS5_VABI Sha3Update(EvpMdContext* context, const void* data, std::size_t length) {
    auto& state = State<mbedtls_sha3_context>(context, Sha3Name<TId>());
    return mbedtls_sha3_update(&state, static_cast<const std::uint8_t*>(data), length) == 0 ? 1 : 0;
}

template <mbedtls_sha3_id TId>
int APS5_VABI Sha3Final(EvpMdContext* context, unsigned char* digest) {
    RequireOutput(digest, Sha3Name<TId>());
    auto& state = State<mbedtls_sha3_context>(context, Sha3Name<TId>());
    return mbedtls_sha3_finish(&state, digest, Sha3Length<TId>()) == 0 ? 1 : 0;
}

template <mbedtls_sha3_id TId>
constexpr EvpMd Sha3(std::int32_t type, std::int32_t pkeyType, std::int32_t blockSize) {
    return {
        type, pkeyType, static_cast<std::int32_t>(Sha3Length<TId>()), DigestAlgorithmIdAbsent, Sha3Init<TId>, Sha3Update<TId>,
        Sha3Final<TId>, nullptr, nullptr, nullptr, nullptr, {PkeyRsa, PkeyRsa2, 0, 0, 0}, blockSize,
        PointerSize + Sha3ContextSize, nullptr,
    };
}

constexpr EvpMd Sha512_224 {
    NidSha512_224, NidSha512_224WithRsa, 28, DigestAlgorithmIdAbsent, Sha512_224Init, Sha512_224Update, Sha512_224Final,
    nullptr, nullptr, nullptr, nullptr, {PkeyRsa, PkeyRsa2, 0, 0, 0}, 128,
    PointerSize + static_cast<std::int32_t>(sizeof(Sha512Context)), nullptr,
};

constexpr EvpMd Ripemd160 {
    NidRipemd160, NidRipemd160WithRsa, 20, 0, Ripemd160Init, Ripemd160Update, Ripemd160Final, nullptr, nullptr, nullptr,
    nullptr, {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, PointerSize + Ripemd160ContextSize, nullptr,
};

constexpr EvpMd Md5Sha1 {
    NidMd5Sha1, NidMd5Sha1, 36, 0, Md5Sha1Init, Md5Sha1Update, Md5Sha1Final, nullptr, nullptr, nullptr, nullptr,
    {0, 0, 0, 0, 0}, 64, PointerSize + static_cast<std::int32_t>(sizeof(Md5Sha1State)), nullptr,
};

constexpr EvpMd Md4 {
    NidMd4, NidMd4WithRsa, 16, 0, Md4Init, Md4Update, Md4Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, PointerSize + static_cast<std::int32_t>(sizeof(Md4State)), nullptr,
};

constexpr EvpMd Sm3 {
    NidSm3, NidSm3WithRsa, 32, DigestAlgorithmIdAbsent, Sm3Init, Sm3Update, Sm3Final, nullptr, nullptr, nullptr, nullptr,
    {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, PointerSize + static_cast<std::int32_t>(sizeof(Sm3State)), nullptr,
};

constexpr EvpMd Sha3_224 = Sha3<MBEDTLS_SHA3_224>(NidSha3_224, NidRsaSha3_224, 144);
constexpr EvpMd Sha3_256 = Sha3<MBEDTLS_SHA3_256>(NidSha3_256, NidRsaSha3_256, 136);
constexpr EvpMd Sha3_384 = Sha3<MBEDTLS_SHA3_384>(NidSha3_384, NidRsaSha3_384, 104);
constexpr EvpMd Sha3_512 = Sha3<MBEDTLS_SHA3_512>(NidSha3_512, NidRsaSha3_512, 72);

} // namespace

namespace LibreSsl3 {

const EvpMd* Sha512_224Digest() {
    return &Sha512_224;
}

const EvpMd* Ripemd160Digest() {
    return &Ripemd160;
}

const EvpMd* Md4Digest() {
    return &Md4;
}

const EvpMd* Sm3Digest() {
    return &Sm3;
}

const EvpMd* Md5Sha1Digest() {
    return &Md5Sha1;
}

const EvpMd* Sha3_224Digest() {
    return &Sha3_224;
}

const EvpMd* Sha3_256Digest() {
    return &Sha3_256;
}

const EvpMd* Sha3_384Digest() {
    return &Sha3_384;
}

const EvpMd* Sha3_512Digest() {
    return &Sha3_512;
}

} // namespace LibreSsl3
