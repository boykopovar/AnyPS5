#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/EvpDigest.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"
#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"

struct LibreSsl3Pkcs8PrivKeyInfo {
    Asn1String* version;
    LibreSsl3X509Algor* algorithm;
    Asn1String* privateKey;
    void* attributes;
};

static_assert(sizeof(LibreSsl3Pkcs8PrivKeyInfo) == 32);
static_assert(offsetof(LibreSsl3Pkcs8PrivKeyInfo, version) == 0);
static_assert(offsetof(LibreSsl3Pkcs8PrivKeyInfo, algorithm) == 8);
static_assert(offsetof(LibreSsl3Pkcs8PrivKeyInfo, privateKey) == 16);
static_assert(offsetof(LibreSsl3Pkcs8PrivKeyInfo, attributes) == 24);

struct LibreSsl3RsaKeyContext {
    std::int32_t keyBits;
    LibreSsl3Bignum* publicExponent;
    std::int32_t generationTemporary[2];
    std::int32_t padding;
    const LibreSsl3::DigestMethod* digest;
    const LibreSsl3::DigestMethod* maskDigest;
    std::int32_t saltLength;
    std::int32_t minimumSaltLength;
    unsigned char* temporaryBuffer;
    unsigned char* oaepLabel;
    std::size_t oaepLabelLength;
};

static_assert(sizeof(LibreSsl3RsaKeyContext) == 80);
static_assert(offsetof(LibreSsl3RsaKeyContext, keyBits) == 0);
static_assert(offsetof(LibreSsl3RsaKeyContext, publicExponent) == 8);
static_assert(offsetof(LibreSsl3RsaKeyContext, generationTemporary) == 16);
static_assert(offsetof(LibreSsl3RsaKeyContext, padding) == 24);
static_assert(offsetof(LibreSsl3RsaKeyContext, digest) == 32);
static_assert(offsetof(LibreSsl3RsaKeyContext, maskDigest) == 40);
static_assert(offsetof(LibreSsl3RsaKeyContext, saltLength) == 48);
static_assert(offsetof(LibreSsl3RsaKeyContext, minimumSaltLength) == 52);
static_assert(offsetof(LibreSsl3RsaKeyContext, temporaryBuffer) == 56);
static_assert(offsetof(LibreSsl3RsaKeyContext, oaepLabel) == 64);
static_assert(offsetof(LibreSsl3RsaKeyContext, oaepLabelLength) == 72);

inline constexpr std::int32_t LibreSsl3RsaPaddingPkcs1 = 1;
inline constexpr std::int32_t LibreSsl3RsaPaddingSslv23 = 2;
inline constexpr std::int32_t LibreSsl3RsaPaddingNone = 3;
inline constexpr std::int32_t LibreSsl3RsaPaddingOaep = 4;
inline constexpr std::int32_t LibreSsl3RsaPaddingX931 = 5;
inline constexpr std::int32_t LibreSsl3RsaPaddingPss = 6;
inline constexpr std::int32_t LibreSsl3RsaPkcs1PaddingSize = 11;
inline constexpr std::int32_t LibreSsl3RsaMinimumModulusBits = 512;
inline constexpr std::int32_t LibreSsl3RsaPssSaltLengthDigest = -1;
inline constexpr std::int32_t LibreSsl3RsaPssSaltLengthAuto = -2;
inline constexpr std::int32_t LibreSsl3RsaPssSaltLengthMaximum = -3;

inline constexpr std::int32_t LibreSsl3RsaFlagCachePublic = 0x0002;
inline constexpr std::int32_t LibreSsl3RsaFlagCachePrivate = 0x0004;
inline constexpr std::int32_t LibreSsl3RsaFlagExternalPrivateKey = 0x0020;
inline constexpr std::int32_t LibreSsl3RsaFlagSignVerify = 0x0040;
inline constexpr std::int32_t LibreSsl3RsaFlagNoBlinding = 0x0080;

inline constexpr std::int32_t LibreSsl3MaximumDigestSize = 64;

bool IsNativePkeyMethod_nid_no_patch(const void* method);
