#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

namespace {

enum class Digest {
    Md4,
    Md5,
    Md5Sha1,
    Ripemd160,
    Sha1,
    Sha224,
    Sha256,
    Sha384,
    Sha512,
    Sha512_224,
    Sha512_256,
    Sha3_224,
    Sha3_256,
    Sha3_384,
    Sha3_512,
    Sm3,
};

struct DigestName {
    const char* name;
    Digest digest;
};

constexpr DigestName Names[] {
    {"DSA-SHA1", Digest::Sha1},
    {"MD4", Digest::Md4},
    {"MD5", Digest::Md5},
    {"MD5-SHA1", Digest::Md5Sha1},
    {"RIPEMD160", Digest::Ripemd160},
    {"RSA-MD4", Digest::Md4},
    {"RSA-MD5", Digest::Md5},
    {"RSA-RIPEMD160", Digest::Ripemd160},
    {"RSA-SHA1", Digest::Sha1},
    {"RSA-SHA1-2", Digest::Sha1},
    {"RSA-SHA224", Digest::Sha224},
    {"RSA-SHA256", Digest::Sha256},
    {"RSA-SHA3-224", Digest::Sha3_224},
    {"RSA-SHA3-256", Digest::Sha3_256},
    {"RSA-SHA3-384", Digest::Sha3_384},
    {"RSA-SHA3-512", Digest::Sha3_512},
    {"RSA-SHA384", Digest::Sha384},
    {"RSA-SHA512", Digest::Sha512},
    {"RSA-SHA512/224", Digest::Sha512_224},
    {"RSA-SHA512/256", Digest::Sha512_256},
    {"RSA-SM3", Digest::Sm3},
    {"SHA1", Digest::Sha1},
    {"SHA224", Digest::Sha224},
    {"SHA256", Digest::Sha256},
    {"SHA3-224", Digest::Sha3_224},
    {"SHA3-256", Digest::Sha3_256},
    {"SHA3-384", Digest::Sha3_384},
    {"SHA3-512", Digest::Sha3_512},
    {"SHA384", Digest::Sha384},
    {"SHA512", Digest::Sha512},
    {"SHA512-224", Digest::Sha512_224},
    {"SHA512-256", Digest::Sha512_256},
    {"SM3", Digest::Sm3},
    {"dsaWithSHA1", Digest::Sha1},
    {"dsa_with_SHA224", Digest::Sha224},
    {"dsa_with_SHA256", Digest::Sha256},
    {"dsa_with_SHA384", Digest::Sha384},
    {"dsa_with_SHA512", Digest::Sha512},
    {"ecdsa-with-SHA1", Digest::Sha1},
    {"ecdsa-with-SHA224", Digest::Sha224},
    {"ecdsa-with-SHA256", Digest::Sha256},
    {"ecdsa-with-SHA384", Digest::Sha384},
    {"ecdsa-with-SHA512", Digest::Sha512},
    {"id-dsa-with-sha224", Digest::Sha224},
    {"id-dsa-with-sha256", Digest::Sha256},
    {"id-dsa-with-sha3-224", Digest::Sha3_224},
    {"id-dsa-with-sha3-256", Digest::Sha3_256},
    {"id-dsa-with-sha3-384", Digest::Sha3_384},
    {"id-dsa-with-sha3-512", Digest::Sha3_512},
    {"id-dsa-with-sha384", Digest::Sha384},
    {"id-dsa-with-sha512", Digest::Sha512},
    {"id-ecdsa-with-sha3-224", Digest::Sha3_224},
    {"id-ecdsa-with-sha3-256", Digest::Sha3_256},
    {"id-ecdsa-with-sha3-384", Digest::Sha3_384},
    {"id-ecdsa-with-sha3-512", Digest::Sha3_512},
    {"id-rsassa-pkcs1-v1_5-with-sha3-224", Digest::Sha3_224},
    {"id-rsassa-pkcs1-v1_5-with-sha3-256", Digest::Sha3_256},
    {"id-rsassa-pkcs1-v1_5-with-sha3-384", Digest::Sha3_384},
    {"id-rsassa-pkcs1-v1_5-with-sha3-512", Digest::Sha3_512},
    {"md4", Digest::Md4},
    {"md4WithRSAEncryption", Digest::Md4},
    {"md5", Digest::Md5},
    {"md5-sha1", Digest::Md5Sha1},
    {"md5WithRSAEncryption", Digest::Md5},
    {"ripemd", Digest::Ripemd160},
    {"ripemd160", Digest::Ripemd160},
    {"ripemd160WithRSA", Digest::Ripemd160},
    {"rmd160", Digest::Ripemd160},
    {"sha1", Digest::Sha1},
    {"sha1WithRSAEncryption", Digest::Sha1},
    {"sha224", Digest::Sha224},
    {"sha224WithRSAEncryption", Digest::Sha224},
    {"sha256", Digest::Sha256},
    {"sha256WithRSAEncryption", Digest::Sha256},
    {"sha3-224", Digest::Sha3_224},
    {"sha3-256", Digest::Sha3_256},
    {"sha3-384", Digest::Sha3_384},
    {"sha3-512", Digest::Sha3_512},
    {"sha384", Digest::Sha384},
    {"sha384WithRSAEncryption", Digest::Sha384},
    {"sha512", Digest::Sha512},
    {"sha512-224", Digest::Sha512_224},
    {"sha512-224WithRSAEncryption", Digest::Sha512_224},
    {"sha512-256", Digest::Sha512_256},
    {"sha512-256WithRSAEncryption", Digest::Sha512_256},
    {"sha512WithRSAEncryption", Digest::Sha512},
    {"sm3", Digest::Sm3},
    {"sm3WithRSAEncryption", Digest::Sm3},
    {"ssl2-md5", Digest::Md5},
    {"ssl3-md5", Digest::Md5},
    {"ssl3-sha1", Digest::Sha1},
};

constexpr bool Sorted() {
    for (std::size_t index = 1; index < std::size(Names); ++index) {
        const char* previous = Names[index - 1].name;
        const char* current = Names[index].name;
        while (*previous && *previous == *current) {
            ++previous;
            ++current;
        }
        if (static_cast<unsigned char>(*previous) >= static_cast<unsigned char>(*current)) return false;
    }
    return true;
}

static_assert(Sorted());

[[noreturn]] void Missing(const char* getter) {
    NotImplemented_nid_no_patch((std::string("EVP_get_digestbyname: ") + getter).c_str());
}

const void* Resolve(Digest digest) {
    switch (digest) {
    case Digest::Md5:
        return EVP_md5_nid_postfix();
    case Digest::Sha1:
        return EVP_sha1_nid_postfix();
    case Digest::Sha224:
        return EVP_sha224_nid_postfix();
    case Digest::Sha256:
        return EVP_sha256_nid_postfix();
    case Digest::Sha384:
        return EVP_sha384_nid_postfix();
    case Digest::Sha512:
        return EVP_sha512_nid_postfix();
    case Digest::Md4:
        Missing("EVP_md4");
    case Digest::Md5Sha1:
        Missing("EVP_md5_sha1");
    case Digest::Ripemd160:
        Missing("EVP_ripemd160");
    case Digest::Sha512_224:
        Missing("EVP_sha512_224");
    case Digest::Sha512_256:
        Missing("EVP_sha512_256");
    case Digest::Sha3_224:
        Missing("EVP_sha3_224");
    case Digest::Sha3_256:
        Missing("EVP_sha3_256");
    case Digest::Sha3_384:
        Missing("EVP_sha3_384");
    case Digest::Sha3_512:
        Missing("EVP_sha3_512");
    case Digest::Sm3:
        Missing("EVP_sm3");
    }
    Missing("unknown digest");
}

} // namespace

extern "C" {

const void* APS5_VABI EVP_get_digestbyname_nid_postfix(const char* name) {
    if (!name) return nullptr;
    const auto* entry = std::lower_bound(std::begin(Names), std::end(Names), name,
        [](const DigestName& candidate, const char* key) { return std::strcmp(candidate.name, key) < 0; });
    if (entry == std::end(Names) || std::strcmp(entry->name, name) != 0) return nullptr;
    return Resolve(entry->digest);
}

}
