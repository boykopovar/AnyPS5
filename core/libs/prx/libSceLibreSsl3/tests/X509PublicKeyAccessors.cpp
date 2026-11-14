#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_get0_pubkey check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3EvpPkey pkey{};
    LibreSsl3X509Pubkey publicKey{};
    publicKey.pkey = &pkey;
    LibreSsl3CertificateInfo info{};
    info.key = &publicKey;
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;

    Require(X509_get0_pubkey_nid_postfix(&certificate) == &pkey);
    publicKey.pkey = nullptr;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    LibreSsl3Asn1String encoded{};
    publicKey.publicKey = &encoded;
    bool threw = false;
    try { X509_get0_pubkey_nid_postfix(&certificate); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    publicKey.publicKey = nullptr;
    info.key = nullptr;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(X509_get0_pubkey_nid_postfix(nullptr) == nullptr);
    std::puts("libSceLibreSsl3 X509_get0_pubkey tests passed");
}
