#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 inventory-name check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    std::uint64_t limbs[1] = {0x8000000000000001ULL};
    Bignum value{};
    value.d = limbs;
    value.top = 1;
    value.dmax = 1;
    value.neg = 1;
    value.flags = 0;
    Require(BN_is_negative_nid_postfix(&value) == 1);
    value.neg = 0;
    Require(BN_is_negative_nid_postfix(&value) == 0);

    LibreSsl3Rsa rsa{};
    rsa.n = &value;
    Require(RSA_bits_nid_postfix(&rsa) == 64);
    rsa.n = nullptr;
    Require(RSA_bits_nid_postfix(&rsa) == 0);

    LibreSsl3EvpCipher cipher{};
    cipher.nid = 0x1234;
    Require(EVP_CIPHER_nid_nid_postfix(&cipher) == 0x1234);

    bool threw = false;
    try {
        BN_is_negative_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        RSA_bits_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        EVP_CIPHER_nid_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    ERR_load_ERR_strings_nid_postfix();
    std::puts("libSceLibreSsl3 inventory-name tests passed");
}
