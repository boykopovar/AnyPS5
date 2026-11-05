#include "prx/libSceLibreSsl3/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ASN1 free check failed at line %u\n", location.line());
    std::abort();
}

void TestOcspCertidFreeNull() {
    bool threw = false;
    try {
        OCSP_CERTID_free_nid_postfix(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(!threw);
}

void TestPkcs7FreeNull() {
    bool threw = false;
    try {
        PKCS7_free_nid_postfix(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(!threw);
}

void TestPkcs8PrivKeyInfoFreeNull() {
    bool threw = false;
    try {
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(!threw);
}

}

int main() {
    TestOcspCertidFreeNull();
    TestPkcs7FreeNull();
    TestPkcs8PrivKeyInfoFreeNull();
    std::puts("libSceLibreSsl3 ASN1 free tests passed");
    return EXIT_SUCCESS;
}
