#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY base check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3EvpPkey key{};
    key.type = 408;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 408);
    key.type = 6;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 6);
    key.type = 19;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 6);
    key.type = 66;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 116);
    key.type = 1000000;
    Require(EVP_PKEY_base_id_nid_postfix(&key) == 0);
    Require(EVP_PKEY_base_id_nid_postfix(nullptr) == 0);
    std::puts("libSceLibreSsl3 EVP_PKEY base tests passed");
}
