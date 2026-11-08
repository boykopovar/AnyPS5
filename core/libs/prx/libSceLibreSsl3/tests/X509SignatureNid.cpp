#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 signature-nid check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    Asn1Object_nid_no_patch object{};
    object.nid = 672;
    LibreSsl3X509Algor algorithm{};
    algorithm.algorithm = &object;
    LibreSsl3X509 certificate{};
    certificate.sigAlg = &algorithm;
    Require(X509_get_signature_nid_nid_postfix(&certificate) == 672);
    const unsigned char oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
    object.nid = 0;
    object.length = 9;
    object.data = oid;
    bool threw = false;
    try { X509_get_signature_nid_nid_postfix(&certificate); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    algorithm.algorithm = nullptr;
    Require(X509_get_signature_nid_nid_postfix(&certificate) == 0);
    certificate.sigAlg = nullptr;
    Require(X509_get_signature_nid_nid_postfix(&certificate) == 0);
    Require(X509_get_signature_nid_nid_postfix(nullptr) == 0);
    std::puts("libSceLibreSsl3 X509 signature-nid tests passed");
}
