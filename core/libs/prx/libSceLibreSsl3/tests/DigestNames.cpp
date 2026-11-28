#include "prx/libSceLibreSsl3/include/ExtraDigests.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <mbedtls/md.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 digest names check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free(void* memory) {
    std::free(memory);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate(void* memory, std::size_t size) {
    return std::realloc(memory, size);
}

void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) {
    return std::realloc(memory, size);
}

int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

struct Expected {
    const char* name;
    const char* getter;
};

constexpr Expected Table[] {
    {"DSA-SHA1", "EVP_sha1"},
    {"MD4", "EVP_md4"},
    {"MD5", "EVP_md5"},
    {"MD5-SHA1", "EVP_md5_sha1"},
    {"RIPEMD160", "EVP_ripemd160"},
    {"RSA-MD4", "EVP_md4"},
    {"RSA-MD5", "EVP_md5"},
    {"RSA-RIPEMD160", "EVP_ripemd160"},
    {"RSA-SHA1", "EVP_sha1"},
    {"RSA-SHA1-2", "EVP_sha1"},
    {"RSA-SHA224", "EVP_sha224"},
    {"RSA-SHA256", "EVP_sha256"},
    {"RSA-SHA3-224", "EVP_sha3_224"},
    {"RSA-SHA3-256", "EVP_sha3_256"},
    {"RSA-SHA3-384", "EVP_sha3_384"},
    {"RSA-SHA3-512", "EVP_sha3_512"},
    {"RSA-SHA384", "EVP_sha384"},
    {"RSA-SHA512", "EVP_sha512"},
    {"RSA-SHA512/224", "EVP_sha512_224"},
    {"RSA-SHA512/256", "EVP_sha512_256"},
    {"RSA-SM3", "EVP_sm3"},
    {"SHA1", "EVP_sha1"},
    {"SHA224", "EVP_sha224"},
    {"SHA256", "EVP_sha256"},
    {"SHA3-224", "EVP_sha3_224"},
    {"SHA3-256", "EVP_sha3_256"},
    {"SHA3-384", "EVP_sha3_384"},
    {"SHA3-512", "EVP_sha3_512"},
    {"SHA384", "EVP_sha384"},
    {"SHA512", "EVP_sha512"},
    {"SHA512-224", "EVP_sha512_224"},
    {"SHA512-256", "EVP_sha512_256"},
    {"SM3", "EVP_sm3"},
    {"dsaWithSHA1", "EVP_sha1"},
    {"dsa_with_SHA224", "EVP_sha224"},
    {"dsa_with_SHA256", "EVP_sha256"},
    {"dsa_with_SHA384", "EVP_sha384"},
    {"dsa_with_SHA512", "EVP_sha512"},
    {"ecdsa-with-SHA1", "EVP_sha1"},
    {"ecdsa-with-SHA224", "EVP_sha224"},
    {"ecdsa-with-SHA256", "EVP_sha256"},
    {"ecdsa-with-SHA384", "EVP_sha384"},
    {"ecdsa-with-SHA512", "EVP_sha512"},
    {"id-dsa-with-sha224", "EVP_sha224"},
    {"id-dsa-with-sha256", "EVP_sha256"},
    {"id-dsa-with-sha3-224", "EVP_sha3_224"},
    {"id-dsa-with-sha3-256", "EVP_sha3_256"},
    {"id-dsa-with-sha3-384", "EVP_sha3_384"},
    {"id-dsa-with-sha3-512", "EVP_sha3_512"},
    {"id-dsa-with-sha384", "EVP_sha384"},
    {"id-dsa-with-sha512", "EVP_sha512"},
    {"id-ecdsa-with-sha3-224", "EVP_sha3_224"},
    {"id-ecdsa-with-sha3-256", "EVP_sha3_256"},
    {"id-ecdsa-with-sha3-384", "EVP_sha3_384"},
    {"id-ecdsa-with-sha3-512", "EVP_sha3_512"},
    {"id-rsassa-pkcs1-v1_5-with-sha3-224", "EVP_sha3_224"},
    {"id-rsassa-pkcs1-v1_5-with-sha3-256", "EVP_sha3_256"},
    {"id-rsassa-pkcs1-v1_5-with-sha3-384", "EVP_sha3_384"},
    {"id-rsassa-pkcs1-v1_5-with-sha3-512", "EVP_sha3_512"},
    {"md4", "EVP_md4"},
    {"md4WithRSAEncryption", "EVP_md4"},
    {"md5", "EVP_md5"},
    {"md5-sha1", "EVP_md5_sha1"},
    {"md5WithRSAEncryption", "EVP_md5"},
    {"ripemd", "EVP_ripemd160"},
    {"ripemd160", "EVP_ripemd160"},
    {"ripemd160WithRSA", "EVP_ripemd160"},
    {"rmd160", "EVP_ripemd160"},
    {"sha1", "EVP_sha1"},
    {"sha1WithRSAEncryption", "EVP_sha1"},
    {"sha224", "EVP_sha224"},
    {"sha224WithRSAEncryption", "EVP_sha224"},
    {"sha256", "EVP_sha256"},
    {"sha256WithRSAEncryption", "EVP_sha256"},
    {"sha3-224", "EVP_sha3_224"},
    {"sha3-256", "EVP_sha3_256"},
    {"sha3-384", "EVP_sha3_384"},
    {"sha3-512", "EVP_sha3_512"},
    {"sha384", "EVP_sha384"},
    {"sha384WithRSAEncryption", "EVP_sha384"},
    {"sha512", "EVP_sha512"},
    {"sha512-224", "EVP_sha512_224"},
    {"sha512-224WithRSAEncryption", "EVP_sha512_224"},
    {"sha512-256", "EVP_sha512_256"},
    {"sha512-256WithRSAEncryption", "EVP_sha512_256"},
    {"sha512WithRSAEncryption", "EVP_sha512"},
    {"sm3", "EVP_sm3"},
    {"sm3WithRSAEncryption", "EVP_sm3"},
    {"ssl2-md5", "EVP_md5"},
    {"ssl3-md5", "EVP_md5"},
    {"ssl3-sha1", "EVP_sha1"},
};

struct Available {
    const char* getter;
    const void* digest;
    mbedtls_md_type_t reference;
    std::vector<unsigned char> expected;
};

const std::vector<unsigned char> Sha512_256Expected {
    0xdd, 0x9d, 0x67, 0xb3, 0x71, 0x51, 0x9c, 0x33, 0x9e, 0xd8, 0xdb, 0xd2, 0x5a, 0xf9, 0x0e, 0x97,
    0x6a, 0x1e, 0xee, 0xfd, 0x4a, 0xd3, 0xd8, 0x89, 0x00, 0x5e, 0x53, 0x2f, 0xc5, 0xbe, 0xf0, 0x4d,
};

const std::vector<unsigned char> Sha512_224Expected {
    0x94, 0x4c, 0xd2, 0x84, 0x7f, 0xb5, 0x45, 0x58, 0xd4, 0x77, 0x5d, 0xb0, 0x48, 0x5a, 0x50, 0x00,
    0x31, 0x11, 0xc8, 0xe5, 0xda, 0xa6, 0x3f, 0xe7, 0x22, 0xc6, 0xaa, 0x37,
};

const std::vector<unsigned char> Ripemd160Expected {
    0x37, 0xf3, 0x32, 0xf6, 0x8d, 0xb7, 0x7b, 0xd9, 0xd7, 0xed, 0xd4, 0x96, 0x95, 0x71, 0xad, 0x67,
    0x1c, 0xf9, 0xdd, 0x3b,
};

const std::vector<unsigned char> Md5Sha1Expected {
    0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82, 0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6,
    0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39,
    0x1b, 0x93, 0xeb, 0x12,
};

const std::vector<unsigned char> Sha3_224Expected {
    0xd1, 0x5d, 0xad, 0xce, 0xaa, 0x4d, 0x5d, 0x7b, 0xb3, 0xb4, 0x8f, 0x44, 0x64, 0x21, 0xd5, 0x42,
    0xe0, 0x8a, 0xd8, 0x88, 0x73, 0x05, 0xe2, 0x8d, 0x58, 0x33, 0x57, 0x95,
};

const std::vector<unsigned char> Sha3_256Expected {
    0x69, 0x07, 0x0d, 0xda, 0x01, 0x97, 0x5c, 0x8c, 0x12, 0x0c, 0x3a, 0xad, 0xa1, 0xb2, 0x82, 0x39,
    0x4e, 0x7f, 0x03, 0x2f, 0xa9, 0xcf, 0x32, 0xf4, 0xcb, 0x22, 0x59, 0xa0, 0x89, 0x7d, 0xfc, 0x04,
};

const std::vector<unsigned char> Sha3_384Expected {
    0x70, 0x63, 0x46, 0x5e, 0x08, 0xa9, 0x3b, 0xce, 0x31, 0xcd, 0x89, 0xd2, 0xe3, 0xca, 0x8f, 0x60,
    0x24, 0x98, 0x69, 0x6e, 0x25, 0x35, 0x92, 0xed, 0x26, 0xf0, 0x7b, 0xf7, 0xe7, 0x03, 0xcf, 0x32,
    0x85, 0x81, 0xe1, 0x47, 0x1a, 0x7b, 0xa7, 0xab, 0x11, 0x9b, 0x1a, 0x9e, 0xbd, 0xf8, 0xbe, 0x41,
};

const std::vector<unsigned char> Sha3_512Expected {
    0x01, 0xde, 0xdd, 0x5d, 0xe4, 0xef, 0x14, 0x64, 0x24, 0x45, 0xba, 0x5f, 0x5b, 0x97, 0xc1, 0x5e,
    0x47, 0xb9, 0xad, 0x93, 0x13, 0x26, 0xe4, 0xb0, 0x72, 0x7c, 0xd9, 0x4c, 0xef, 0xc4, 0x4f, 0xff,
    0x23, 0xf0, 0x7b, 0xf5, 0x43, 0x13, 0x99, 0x39, 0xb4, 0x91, 0x28, 0xca, 0xf4, 0x36, 0xdc, 0x1b,
    0xde, 0xe5, 0x4f, 0xcb, 0x24, 0x02, 0x3a, 0x08, 0xd9, 0x40, 0x3f, 0x9b, 0x4b, 0xf0, 0xd4, 0x50,
};

std::vector<Available> AvailableDigests() {
    return {
        {"EVP_md5", EVP_md5_nid_postfix(), MBEDTLS_MD_MD5, {}},
        {"EVP_sha1", EVP_sha1_nid_postfix(), MBEDTLS_MD_SHA1, {}},
        {"EVP_sha224", EVP_sha224_nid_postfix(), MBEDTLS_MD_SHA224, {}},
        {"EVP_sha256", EVP_sha256_nid_postfix(), MBEDTLS_MD_SHA256, {}},
        {"EVP_sha384", EVP_sha384_nid_postfix(), MBEDTLS_MD_SHA384, {}},
        {"EVP_sha512", EVP_sha512_nid_postfix(), MBEDTLS_MD_SHA512, {}},
        {"EVP_sha512_256", EVP_sha512_256_nid_postfix(), MBEDTLS_MD_NONE, Sha512_256Expected},
        {"EVP_sha512_224", LibreSsl3::Sha512_224Digest(), MBEDTLS_MD_NONE, Sha512_224Expected},
        {"EVP_ripemd160", LibreSsl3::Ripemd160Digest(), MBEDTLS_MD_NONE, Ripemd160Expected},
        {"EVP_md5_sha1", LibreSsl3::Md5Sha1Digest(), MBEDTLS_MD_NONE, Md5Sha1Expected},
        {"EVP_sha3_224", LibreSsl3::Sha3_224Digest(), MBEDTLS_MD_NONE, Sha3_224Expected},
        {"EVP_sha3_256", LibreSsl3::Sha3_256Digest(), MBEDTLS_MD_NONE, Sha3_256Expected},
        {"EVP_sha3_384", LibreSsl3::Sha3_384Digest(), MBEDTLS_MD_NONE, Sha3_384Expected},
        {"EVP_sha3_512", LibreSsl3::Sha3_512Digest(), MBEDTLS_MD_NONE, Sha3_512Expected},
        {"EVP_md4", LibreSsl3::Md4Digest(), MBEDTLS_MD_NONE, {
            0x1b, 0xee, 0x69, 0xa4, 0x6b, 0xa8, 0x11, 0x18, 0x5c, 0x19, 0x47, 0x62, 0xab, 0xae, 0xae, 0x90}},
        {"EVP_sm3", LibreSsl3::Sm3Digest(), MBEDTLS_MD_NONE, {
            0x5f, 0xdf, 0xe8, 0x14, 0xb8, 0x57, 0x3c, 0xa0, 0x21, 0x98, 0x39, 0x70, 0xfc, 0x79, 0xb2, 0x21,
            0x8c, 0x95, 0x70, 0x36, 0x9b, 0x48, 0x59, 0x68, 0x4e, 0x2e, 0x4c, 0x3f, 0xc7, 0x6c, 0xb8, 0xea}},
    };
}

const Available* Find(const char* getter) {
    static const auto digests = AvailableDigests();
    for (const auto& digest : digests)
        if (std::strcmp(digest.getter, getter) == 0) return &digest;
    return nullptr;
}

void CheckEveryName() {
    const unsigned char data[] = "The quick brown fox jumps over the lazy dog";
    int available = 0;
    for (const auto& entry : Table) {
        const auto* digest = Find(entry.getter);
        Require(digest != nullptr);
        const void* found = EVP_get_digestbyname_nid_postfix(entry.name);
        Require(found == digest->digest);
        unsigned char output[64];
        std::uint32_t length = 0;
        Require(EVP_Digest_nid_postfix(data, sizeof(data) - 1, output, &length, static_cast<const EvpMd*>(found), nullptr) == 1);
        if (digest->reference == MBEDTLS_MD_NONE) {
            Require(length == digest->expected.size());
            Require(std::memcmp(output, digest->expected.data(), length) == 0);
        } else {
            const mbedtls_md_info_t* info = mbedtls_md_info_from_type(digest->reference);
            unsigned char expected[64];
            Require(mbedtls_md(info, data, sizeof(data) - 1, expected) == 0);
            Require(length == mbedtls_md_get_size(info));
            Require(std::memcmp(output, expected, length) == 0);
        }
        ++available;
    }
    Require(available == 91);
}

void RequireDigest(const EvpMd* digest, const void* data, std::size_t size, const std::vector<unsigned char>& expected) {
    unsigned char output[64];
    std::uint32_t length = 0;
    Require(EVP_Digest_nid_postfix(data, size, output, &length, digest, nullptr) == 1);
    Require(length == expected.size() && std::memcmp(output, expected.data(), length) == 0);
}

void CheckMd4AndSm3Vectors() {
    RequireDigest(LibreSsl3::Md4Digest(), "", 0, {
        0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31, 0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0});
    RequireDigest(LibreSsl3::Sm3Digest(), "", 0, {
        0x1a, 0xb2, 0x1d, 0x83, 0x55, 0xcf, 0xa1, 0x7f, 0x8e, 0x61, 0x19, 0x48, 0x31, 0xe8, 0x1a, 0x8f,
        0x22, 0xbe, 0xc8, 0xc7, 0x28, 0xfe, 0xfb, 0x74, 0x7e, 0xd0, 0x35, 0xeb, 0x50, 0x82, 0xaa, 0x2b});
    RequireDigest(LibreSsl3::Md4Digest(), "abc", 3, {
        0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52, 0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d});
    RequireDigest(LibreSsl3::Sm3Digest(), "abc", 3, {
        0x66, 0xc7, 0xf0, 0xf4, 0x62, 0xee, 0xed, 0xd9, 0xd1, 0xf2, 0xd4, 0x6b, 0xdc, 0x10, 0xe4, 0xe2,
        0x41, 0x67, 0xc4, 0x87, 0x5c, 0xf2, 0xf7, 0xa2, 0x29, 0x7d, 0xa0, 0x2b, 0x8f, 0x4b, 0xa8, 0xe0});
    const std::vector<unsigned char> million(1000000, 'a');
    RequireDigest(LibreSsl3::Md4Digest(), million.data(), million.size(), {
        0xbb, 0xce, 0x80, 0xcc, 0x6b, 0xb6, 0x5e, 0x5c, 0x67, 0x45, 0xe3, 0x0d, 0x4e, 0xec, 0xa9, 0xa4});
    RequireDigest(LibreSsl3::Sm3Digest(), million.data(), million.size(), {
        0xc8, 0xaa, 0xf8, 0x94, 0x29, 0x55, 0x40, 0x29, 0xe2, 0x31, 0x94, 0x1a, 0x2a, 0xcc, 0x0a, 0xd6,
        0x1f, 0xf2, 0xa5, 0xac, 0xd8, 0xfa, 0xdd, 0x25, 0x84, 0x7a, 0x3a, 0x73, 0x2b, 0x3b, 0x02, 0xc3});
    for (std::size_t size = 54; size <= 66; ++size) {
        std::vector<unsigned char> boundary(size, 'q');
        unsigned char whole[64];
        unsigned char split[64];
        std::uint32_t length = 0;
        for (const EvpMd* digest : {LibreSsl3::Md4Digest(), LibreSsl3::Sm3Digest()}) {
            Require(EVP_Digest_nid_postfix(boundary.data(), size, whole, &length, digest, nullptr) == 1);
            EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
            Require(context != nullptr);
            Require(EVP_DigestInit_ex_nid_postfix(context, digest, nullptr) == 1);
            for (std::size_t offset = 0; offset < size; ++offset)
                Require(EVP_DigestUpdate_nid_postfix(context, boundary.data() + offset, 1) == 1);
            Require(EVP_DigestUpdate_nid_postfix(context, nullptr, 0) == 1);
            Require(EVP_DigestFinal_ex_nid_postfix(context, split, &length) == 1);
            Require(std::memcmp(whole, split, length) == 0);
            EVP_MD_CTX_destroy_nid_postfix(context);
        }
    }
}

struct Metadata {
    const EvpMd* digest;
    std::int32_t type;
    std::int32_t pkeyType;
    std::int32_t size;
    std::uint64_t flags;
    std::int32_t blockSize;
    std::int32_t contextSize;
};

void CheckMetadata() {
    const Metadata expected[] {
        {LibreSsl3::Sha512_224Digest(), 1029, 1025, 28, 0x8, 128, 224},
        {LibreSsl3::Ripemd160Digest(), 117, 119, 20, 0, 64, 104},
        {LibreSsl3::Md5Sha1Digest(), 114, 114, 36, 0, 64, 196},
        {LibreSsl3::Sha3_224Digest(), 1031, 1049, 28, 0x8, 144, 232},
        {LibreSsl3::Sha3_256Digest(), 1032, 1050, 32, 0x8, 136, 232},
        {LibreSsl3::Sha3_384Digest(), 1033, 1051, 48, 0x8, 104, 232},
        {LibreSsl3::Sha3_512Digest(), 1034, 1052, 64, 0x8, 72, 232},
        {LibreSsl3::Md4Digest(), 257, 396, 16, 0, 64, 100},
        {LibreSsl3::Sm3Digest(), 968, 969, 32, 0x8, 64, 116},
    };
    for (const auto& entry : expected) {
        Require(entry.digest->type == entry.type && entry.digest->pkeyType == entry.pkeyType);
        Require(entry.digest->mdSize == entry.size && entry.digest->flags == entry.flags);
        Require(entry.digest->blockSize == entry.blockSize && entry.digest->contextSize == entry.contextSize);
        Require(!entry.digest->copy && !entry.digest->cleanup && !entry.digest->control);
    }
}

void CheckStreaming() {
    std::vector<unsigned char> data(1280);
    for (std::size_t index = 0; index < data.size(); ++index) data[index] = static_cast<unsigned char>(index);
    const Available sha512_224 {"", LibreSsl3::Sha512_224Digest(), MBEDTLS_MD_NONE, {
        0x3e, 0x50, 0x80, 0x92, 0x9a, 0x0e, 0x5b, 0x04, 0xfd, 0x5d, 0xf7, 0x76, 0x5c, 0x2d, 0x2e, 0x1a,
        0xc4, 0x00, 0x9c, 0x1c, 0x75, 0x7f, 0x4b, 0x95, 0xde, 0x35, 0x92, 0x00}};
    const Available ripemd160 {"", LibreSsl3::Ripemd160Digest(), MBEDTLS_MD_NONE, {
        0x7d, 0x9d, 0xb3, 0xac, 0x93, 0x55, 0xaa, 0xb2, 0xae, 0xd9, 0x28, 0xba, 0x9b, 0xe2, 0xc3, 0xbf,
        0x0e, 0x8c, 0x9d, 0x48}};
    const Available md5Sha1 {"", LibreSsl3::Md5Sha1Digest(), MBEDTLS_MD_NONE, {
        0x82, 0x82, 0x9f, 0x1f, 0x3f, 0x2b, 0xb0, 0xf1, 0x8b, 0x25, 0xf2, 0x78, 0xe5, 0xbb, 0xa8, 0xbd,
        0xe3, 0x7a, 0x04, 0xcb, 0x23, 0x53, 0x30, 0x9f, 0x5c, 0xff, 0x4e, 0xe0, 0x36, 0xcf, 0xb9, 0x1a,
        0x5e, 0x31, 0xce, 0xfd}};
    const Available sha3_256 {"", LibreSsl3::Sha3_256Digest(), MBEDTLS_MD_NONE, {
        0xfe, 0x0f, 0xf5, 0x04, 0xc7, 0x9e, 0x25, 0x1a, 0xed, 0xec, 0x1e, 0x83, 0xdd, 0x99, 0x63, 0x7b,
        0x93, 0x65, 0x0f, 0x1a, 0x15, 0x05, 0x31, 0x0a, 0xcd, 0x4e, 0x77, 0xb0, 0x34, 0xff, 0xdf, 0xca}};
    const Available md4 {"", LibreSsl3::Md4Digest(), MBEDTLS_MD_NONE, {
        0xd7, 0x51, 0xf5, 0x91, 0x12, 0xc9, 0x6d, 0xa0, 0x4e, 0x1c, 0x30, 0xc0, 0xed, 0x9a, 0x0b, 0xa7}};
    const Available sm3 {"", LibreSsl3::Sm3Digest(), MBEDTLS_MD_NONE, {
        0xb6, 0xcc, 0x0a, 0x44, 0x1e, 0x5b, 0x27, 0xfe, 0xab, 0xdc, 0x18, 0x2e, 0xee, 0xcd, 0x50, 0x00,
        0x3a, 0xfa, 0x7d, 0x80, 0x11, 0xc5, 0x4e, 0xa0, 0x88, 0xc3, 0xf5, 0x2f, 0x73, 0x4f, 0xea, 0xf9}};
    for (const Available* digest : {&sha512_224, &ripemd160, &md5Sha1, &sha3_256, &md4, &sm3}) {
        EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
        Require(context != nullptr);
        for (int round = 0; round < 2; ++round) {
            Require(EVP_DigestInit_ex_nid_postfix(context, static_cast<const EvpMd*>(digest->digest), nullptr) == 1);
            for (std::size_t offset = 0, chunk = 1; offset < data.size(); offset += chunk, chunk = chunk * 3 + 1) {
                const std::size_t length = std::min(chunk, data.size() - offset);
                Require(EVP_DigestUpdate_nid_postfix(context, data.data() + offset, length) == 1);
            }
            unsigned char output[64];
            std::uint32_t length = 0;
            Require(EVP_DigestFinal_ex_nid_postfix(context, output, &length) == 1);
            Require(length == digest->expected.size());
            Require(std::memcmp(output, digest->expected.data(), length) == 0);
        }
        EVP_MD_CTX_destroy_nid_postfix(context);
    }
}

void CheckUnknownNames() {
    Require(EVP_get_digestbyname_nid_postfix(nullptr) == nullptr);
    for (const char* name : {"", "sha", "SHA", "sha2560", "SHA256 ", " SHA256", "Sha256", "SHA-256", "sha-256", "SHA2-256", "SHA512/256", "md2", "MD2", "mdc2",
                             "whirlpool", "blake2b512", "shake128", "aes-128-cbc", "AES-256-GCM", "chacha20", "hmacWithSHA256", "RSA-SHA", "ssl3-sha256",
                             "id-dsa-with-sha1", "dsa_with_SHA1", "ecdsa-with-SHA3-256", "DSA-SHA", "DSA", "RSA", "rsaEncryption", "~", "\x7f", "\xff"})
        Require(EVP_get_digestbyname_nid_postfix(name) == nullptr);
}

void CheckLongName() {
    std::string name(4096, 's');
    Require(EVP_get_digestbyname_nid_postfix(name.c_str()) == nullptr);
    name = "sha256";
    name.append(4096, '0');
    Require(EVP_get_digestbyname_nid_postfix(name.c_str()) == nullptr);
}

void CheckRepeatable() {
    for (int round = 0; round < 3; ++round) {
        Require(EVP_get_digestbyname_nid_postfix("SHA256") == EVP_sha256_nid_postfix());
        Require(EVP_get_digestbyname_nid_postfix("ssl3-sha1") == static_cast<const void*>(EVP_sha1_nid_postfix()));
        Require(EVP_get_digestbyname_nid_postfix("ssl2-md5") == EVP_md5_nid_postfix());
        Require(EVP_get_digestbyname_nid_postfix("SHA512-256") == EVP_sha512_256_nid_postfix());
        Require(EVP_get_digestbyname_nid_postfix("RSA-SHA512/256") == EVP_sha512_256_nid_postfix());
        Require(EVP_get_digestbyname_nid_postfix("sha512-256") == EVP_sha512_256_nid_postfix());
        Require(EVP_get_digestbyname_nid_postfix("sha512-256WithRSAEncryption") == EVP_sha512_256_nid_postfix());
    }
}

char* MapPageBeforeGuard() {
#ifdef _WIN32
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<char*>(mapped);
    Require(mprotect(pages + 4096, 4096, PROT_NONE) == 0);
#endif
    return pages;
}

void ProtectPageReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

void UnmapPages(char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

template <typename ExceptionType, typename Callable>
void RequireThrowsPreservingErrno(int expectedErrno, Callable&& call) {
    errno = expectedErrno;
    bool threw = false;
    try {
        call();
    } catch (const ExceptionType&) {
        threw = true;
    }
    Require(threw && errno == expectedErrno);
}

void CheckGuestName() {
    errno = 71;
    bool rejected = false;
    try {
        EVP_get_digestbyname_nid_postfix(reinterpret_cast<const char*>(std::uintptr_t{0x10}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && errno == 71);

    char* pages = MapPageBeforeGuard();
    char* name = pages + 4096 - sizeof("SHA256");
    std::memcpy(name, "SHA256", sizeof("SHA256"));
    Require(name[sizeof("SHA256") - 1] == '\0' && name + sizeof("SHA256") == pages + 4096);
    errno = 71;
    Require(EVP_get_digestbyname_nid_postfix(name) == EVP_sha256_nid_postfix() && errno == 71);
    UnmapPages(pages);
}

void CheckPkeyGet1Rsa() {
    LibreSsl3Rsa rsa{};
    rsa.references = 1;
    LibreSsl3EvpPkey rsaKey{};
    rsaKey.type = 6;
    rsaKey.references = 1;
    rsaKey.key = &rsa;
    ERR_clear_error_nid_postfix();
    errno = 71;
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&rsaKey) == &rsa);
    Require(rsa.references == 2 && ERR_peek_last_error_nid_postfix() == 0 && errno == 71);

    LibreSsl3Rsa rsaPss{};
    rsaPss.references = 1;
    LibreSsl3EvpPkey pssKey{};
    pssKey.type = 912;
    pssKey.references = 1;
    pssKey.key = &rsaPss;
    ERR_clear_error_nid_postfix();
    errno = 71;
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&pssKey) == &rsaPss);
    Require(rsaPss.references == 2 && ERR_peek_last_error_nid_postfix() == 0 && errno == 71);

    LibreSsl3EvpPkey wrongKey{};
    wrongKey.type = 408;
    wrongKey.references = 1;
    wrongKey.key = &rsa;
    ERR_clear_error_nid_postfix();
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&wrongKey) == nullptr);
    Require(rsa.references == 2 && ERR_peek_last_error_nid_postfix() != 0);
    ERR_clear_error_nid_postfix();

    RequireThrowsPreservingErrno<std::invalid_argument>(71, [] {
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(nullptr);
    });
    LibreSsl3EvpPkey emptyRsa{};
    emptyRsa.type = 912;
    emptyRsa.references = 1;
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&emptyRsa);
    });
}

void CheckPkeySet1EcKeyAtomic() {
    LibreSsl3EvpPkey pkey{};
    pkey.references = 1;
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(&pkey, nullptr) == 0);
    Require(pkey.type == 0 && pkey.key == nullptr);

    LibreSsl3EcKey overflowEc{};
    overflowEc.references = std::numeric_limits<std::int32_t>::max();
    RequireThrowsPreservingErrno<std::overflow_error>(71, [&] {
        EVP_PKEY_set1_EC_KEY_nid_postfix(&pkey, &overflowEc);
    });
    Require(pkey.type == 0 && pkey.key == nullptr);
    Require(overflowEc.references == std::numeric_limits<std::int32_t>::max());

    auto* badEc = reinterpret_cast<LibreSsl3EcKey*>(std::uintptr_t{0x10});
    RequireThrowsPreservingErrno<std::runtime_error>(71, [&] {
        EVP_PKEY_set1_EC_KEY_nid_postfix(&pkey, badEc);
    });
    Require(pkey.type == 0 && pkey.key == nullptr);

    LibreSsl3EcKey validEc{};
    validEc.references = 1;
    errno = 71;
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(&pkey, &validEc) == 1);
    Require(pkey.type == 408 && pkey.key == &validEc && validEc.references == 2 && errno == 71);
}

void CheckPkeyGuestPointers() {
    LibreSsl3Rsa rsa{};
    rsa.references = 1;
    LibreSsl3EcKey ec{};
    ec.references = 1;
    auto* unmappedKey = reinterpret_cast<LibreSsl3EvpPkey*>(std::uintptr_t{0x10});

    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(unmappedKey);
    });
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_set1_EC_KEY_nid_postfix(unmappedKey, &ec);
    });
    Require(ec.references == 1);
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_assign_nid_postfix(unmappedKey, 6, &rsa);
    });
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_get0_RSA_nid_postfix(unmappedKey);
    });
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_get0_DSA_nid_postfix(unmappedKey);
    });
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_get0_DH_nid_postfix(unmappedKey);
    });

    char* pages = MapPageBeforeGuard();
    auto* roEmpty = reinterpret_cast<LibreSsl3EvpPkey*>(pages);
    auto* roRsa = reinterpret_cast<LibreSsl3EvpPkey*>(pages + sizeof(LibreSsl3EvpPkey));
    auto* roDsa = reinterpret_cast<LibreSsl3EvpPkey*>(pages + sizeof(LibreSsl3EvpPkey) * 2);
    auto* roDh = reinterpret_cast<LibreSsl3EvpPkey*>(pages + sizeof(LibreSsl3EvpPkey) * 3);
    LibreSsl3EvpPkey emptyInit{};
    emptyInit.references = 1;
    LibreSsl3EvpPkey rsaInit{};
    rsaInit.type = 912;
    rsaInit.references = 1;
    rsaInit.key = &rsa;
    int dsaDummy = 1;
    LibreSsl3EvpPkey dsaInit{};
    dsaInit.type = 116;
    dsaInit.references = 1;
    dsaInit.key = &dsaDummy;
    int dhDummy = 2;
    LibreSsl3EvpPkey dhInit{};
    dhInit.type = 28;
    dhInit.references = 1;
    dhInit.key = &dhDummy;
    std::memcpy(roEmpty, &emptyInit, sizeof(emptyInit));
    std::memcpy(roRsa, &rsaInit, sizeof(rsaInit));
    std::memcpy(roDsa, &dsaInit, sizeof(dsaInit));
    std::memcpy(roDh, &dhInit, sizeof(dhInit));
    ProtectPageReadOnly(pages);

    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_set1_EC_KEY_nid_postfix(roEmpty, &ec);
    });
    Require(ec.references == 1);
    RequireThrowsPreservingErrno<std::invalid_argument>(71, [&] {
        EVP_PKEY_assign_nid_postfix(roEmpty, 6, &rsa);
    });

    ERR_clear_error_nid_postfix();
    errno = 71;
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(roRsa) == &rsa);
    Require(rsa.references == 2 && ERR_peek_last_error_nid_postfix() == 0 && errno == 71);
    Require(EVP_PKEY_get0_RSA_nid_postfix(roRsa) == &rsa && errno == 71);
    Require(EVP_PKEY_get0_DSA_nid_postfix(roDsa) == &dsaDummy && errno == 71);
    Require(EVP_PKEY_get0_DH_nid_postfix(roDh) == &dhDummy && errno == 71);
    Require(EVP_PKEY_get0_DSA_nid_postfix(roRsa) == nullptr && ERR_peek_last_error_nid_postfix() != 0);
    ERR_clear_error_nid_postfix();
    Require(EVP_PKEY_get0_DH_nid_postfix(roRsa) == nullptr && ERR_peek_last_error_nid_postfix() != 0);
    ERR_clear_error_nid_postfix();

    UnmapPages(pages);
}

} // namespace

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckEveryName();
    CheckMetadata();
    CheckMd4AndSm3Vectors();
    CheckStreaming();
    CheckUnknownNames();
    CheckLongName();
    CheckRepeatable();
    CheckGuestName();
    CheckPkeyGet1Rsa();
    CheckPkeySet1EcKeyAtomic();
    CheckPkeyGuestPointers();
    std::puts("LibreSSL3 digest names: ok");
    return 0;
}
