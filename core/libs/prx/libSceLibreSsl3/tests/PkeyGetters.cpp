#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <cstdlib>
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
    int dsa = 0;
    int dh = 0;
    auto rsaKey = KeyWith(6, &rsa);
    auto rsaPssKey = KeyWith(912, &rsa);
    auto dsaKey = KeyWith(116, &dsa);
    auto dhKey = KeyWith(28, &dh);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaKey) == &rsa);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaPssKey) == &rsa);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&dsaKey) == &dsa);
    Require(EVP_PKEY_get0_DH_nid_postfix(&dhKey) == &dh);
    Require(rsaKey.references == 1 && dsaKey.references == 1 && dhKey.references == 1);
    auto emptyRsa = KeyWith(6, nullptr);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&emptyRsa) == nullptr);
}
