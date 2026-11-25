#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY accessor check failed at line %u\n", location.line());
    std::abort();
}

int customFreeCalls;

void APS5_VABI FreeCustom(LibreSsl3EvpPkey*) {
    ++customFreeCalls;
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

    LibreSsl3EvpPkey rsaKey{};
    rsaKey.references = 1;
    LibreSsl3Rsa firstRsa{};
    firstRsa.references = 1;
    LibreSsl3Rsa secondRsa{};
    secondRsa.references = 1;
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&rsaKey, &firstRsa) == 1);
    Require(rsaKey.type == 6 && rsaKey.key == &firstRsa && rsaKey.method != nullptr);
    Require(firstRsa.references == 2);
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&rsaKey, &secondRsa) == 1);
    Require(firstRsa.references == 1 && secondRsa.references == 2);
    Require(rsaKey.key == &secondRsa && rsaKey.method != nullptr);
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&rsaKey, &secondRsa) == 1);
    Require(secondRsa.references == 2 && rsaKey.key == &secondRsa);

    LibreSsl3EvpPkey nullRsaKey{};
    nullRsaKey.references = 1;
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&nullRsaKey, nullptr) == 0);
    Require(nullRsaKey.type == 6 && nullRsaKey.key == nullptr && nullRsaKey.method != nullptr);

    LibreSsl3EcKey replacedEc{};
    replacedEc.references = 1;
    LibreSsl3EvpPkey ecKey{};
    ecKey.references = 1;
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(&ecKey, &replacedEc) == 1);
    Require(replacedEc.references == 2);
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&ecKey, &firstRsa) == 1);
    Require(replacedEc.references == 1 && firstRsa.references == 2);

    LibreSsl3PkeyAsn1Method customMethod{};
    customMethod.keyFree = FreeCustom;
    LibreSsl3EvpPkey customKey{};
    customKey.references = 1;
    customKey.method = &customMethod;
    customKey.key = &customKey;
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&customKey, &firstRsa) == 1);
    Require(customFreeCalls == 1 && customKey.type == 6 && customKey.key == &firstRsa);
    Require(firstRsa.references == 3);

    LibreSsl3EvpPkey overflowKey{};
    overflowKey.references = 1;
    LibreSsl3Rsa overflowRsa{};
    overflowRsa.references = std::numeric_limits<std::int32_t>::max();
    bool overflowed = false;
    try {
        EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(&overflowKey, &overflowRsa);
    } catch (const std::overflow_error&) {
        overflowed = true;
    }
    Require(overflowed && overflowRsa.references == std::numeric_limits<std::int32_t>::max());
    Require(overflowKey.type == 6 && overflowKey.key == &overflowRsa && overflowKey.method != nullptr);

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
    threw = false;
    try {
        EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(nullptr, &rsa);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(ERR_get_error_nid_postfix() == 0);
    std::puts("libSceLibreSsl3 EVP_PKEY accessor tests passed");
}
