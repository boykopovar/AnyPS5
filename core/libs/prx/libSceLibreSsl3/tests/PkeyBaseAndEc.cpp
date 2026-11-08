#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY base/EC check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3EcKey ec{};
    LibreSsl3EvpPkey key{};
    key.type = 408;
    key.key = &ec;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 408);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&key) == &ec);
    key.type = 6;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 6);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&key) == nullptr);
    Require(EVP_PKEY_base_id_nid_postfix(nullptr) == 0);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(nullptr) == nullptr);
    std::puts("libSceLibreSsl3 EVP_PKEY base/EC tests passed");
}
