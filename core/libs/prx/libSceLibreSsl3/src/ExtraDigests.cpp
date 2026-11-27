#include "prx/libSceLibreSsl3/include/ExtraDigests.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <mbedtls/ripemd160.h>
#include <mbedtls/sha3.h>

#include <array>
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
