#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY getter check failed at line %u\n", location.line());
    std::abort();
}

LibreSsl3EvpPkey KeyWith(std::int32_t type, void* inner) {
    LibreSsl3EvpPkey key{};
    key.type = type;
    key.references = 1;
    key.key = inner;
    return key;
}

}

int main() {
    LibreSsl3Rsa rsa{};
    LibreSsl3EcKey ec{};
    int dsa = 0;
    int dh = 0;
    auto rsaKey = KeyWith(6, &rsa);
    auto rsaPssKey = KeyWith(912, &rsa);
    auto dsaKey = KeyWith(116, &dsa);
    auto dhKey = KeyWith(28, &dh);
    auto ecKey = KeyWith(408, &ec);
    ERR_clear_error_nid_postfix();
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaKey) == &rsa);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaPssKey) == &rsa);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&dsaKey) == &dsa);
    Require(EVP_PKEY_get0_DH_nid_postfix(&dhKey) == &dh);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&ecKey) == &ec);
    Require(rsaKey.references == 1 && dsaKey.references == 1 && dhKey.references == 1 && ecKey.references == 1);
    auto emptyRsa = KeyWith(6, nullptr);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&emptyRsa) == nullptr);
    auto emptyEc = KeyWith(408, nullptr);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&emptyEc) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    const auto rejected = [](void* result, std::uint64_t code) {
        Require(result == nullptr);
        Require(ERR_get_error_nid_postfix() == code);
        Require(ERR_get_error_nid_postfix() == 0);
    };
    rejected(EVP_PKEY_get0_RSA_nid_postfix(&dsaKey), 0x06fff07f);
    rejected(EVP_PKEY_get0_RSA_nid_postfix(&ecKey), 0x06fff07f);
    rejected(EVP_PKEY_get0_DSA_nid_postfix(&rsaKey), 0x06fff081);
    rejected(EVP_PKEY_get0_DSA_nid_postfix(&rsaPssKey), 0x06fff081);
    rejected(EVP_PKEY_get0_DH_nid_postfix(&dsaKey), 0x06fff080);
    rejected(EVP_PKEY_get0_EC_KEY_nid_postfix(&rsaKey), 0x06fff08e);
    Require(rsaKey.references == 1 && rsaPssKey.references == 1 && dsaKey.references == 1 && ecKey.references == 1);
    auto untyped = KeyWith(0, &rsa);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&untyped) == nullptr);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&untyped) == nullptr);
    Require(EVP_PKEY_get0_DH_nid_postfix(&untyped) == nullptr);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&untyped) == nullptr);
    for (const std::uint64_t code : {0x06fff07fULL, 0x06fff081ULL, 0x06fff080ULL, 0x06fff08eULL})
        Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0 && untyped.references == 1);
}
