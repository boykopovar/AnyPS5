#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <mbedtls/md.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>
#include <vector>

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
};

std::vector<Available> AvailableDigests() {
    return {
        {"EVP_md5", EVP_md5_nid_postfix(), MBEDTLS_MD_MD5},
        {"EVP_sha1", EVP_sha1_nid_postfix(), MBEDTLS_MD_SHA1},
        {"EVP_sha224", EVP_sha224_nid_postfix(), MBEDTLS_MD_SHA224},
        {"EVP_sha256", EVP_sha256_nid_postfix(), MBEDTLS_MD_SHA256},
        {"EVP_sha384", EVP_sha384_nid_postfix(), MBEDTLS_MD_SHA384},
        {"EVP_sha512", EVP_sha512_nid_postfix(), MBEDTLS_MD_SHA512},
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
        if (const auto* digest = Find(entry.getter)) {
            const void* found = EVP_get_digestbyname_nid_postfix(entry.name);
            Require(found == digest->digest);
            unsigned char output[64];
            std::uint32_t length = 0;
            Require(EVP_Digest_nid_postfix(data, sizeof(data) - 1, output, &length, static_cast<const EvpMd*>(found), nullptr) == 1);
            const mbedtls_md_info_t* info = mbedtls_md_info_from_type(digest->reference);
            unsigned char expected[64];
            Require(mbedtls_md(info, data, sizeof(data) - 1, expected) == 0);
            Require(length == mbedtls_md_get_size(info));
            Require(std::memcmp(output, expected, length) == 0);
            ++available;
        }
    }
    Require(available == 43);
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
    }
}

} // namespace

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckEveryName();
    CheckUnknownNames();
    CheckLongName();
    CheckRepeatable();
    std::puts("LibreSSL3 digest names: ok");
    return 0;
}
