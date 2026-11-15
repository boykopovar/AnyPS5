#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY accessor check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    LibreSsl3EvpPkey key{};
    key.references = 1;
    LibreSsl3Rsa rsa{};
    rsa.references = 1;
    Require(EVP_PKEY_assign_nid_postfix(&key, 6, &rsa) == 1);
    Require(key.type == 6 && key.key == &rsa && key.method == nullptr);
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&key) == &rsa);
    Require(rsa.references == 2);
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&key) == &rsa);
    Require(rsa.references == 3);

    LibreSsl3EcKey ec{};
    ec.references = 1;
    LibreSsl3EvpPkey other{};
    other.references = 1;
    other.type = 408;
    other.key = &ec;
    Require(EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(&other) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0x06fff07f);

    LibreSsl3EvpPkey assigned{};
    assigned.references = 1;
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(&assigned, &ec) == 1);
    Require(assigned.type == 408 && assigned.key == &ec && ec.references == 2);

    LibreSsl3EvpPkey empty{};
    empty.references = 1;
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(&empty, nullptr) == 0);
    Require(empty.type == 0 && empty.key == nullptr);

    bool threw = false;
    try {
        EVP_PKEY_assign_nid_postfix(&key, 408, &ec);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        EVP_PKEY_assign_nid_postfix(nullptr, 6, &rsa);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(ERR_get_error_nid_postfix() == 0);
    std::puts("libSceLibreSsl3 EVP_PKEY accessor tests passed");
}
