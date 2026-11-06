#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

static int Before;
static int After;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509 validity check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGetmNotBefore() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_getm_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == &Before);
    validity.notBefore = nullptr;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
}

static void TestGetmNotAfter() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_getm_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == &After);
    validity.notAfter = nullptr;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
}

static void TestGet0NotBefore() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_get0_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == &Before);
    validity.notBefore = nullptr;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
}

static void TestGet0NotAfter() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_get0_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == &After);
    validity.notAfter = nullptr;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
}

static void TestGet0Signature() {
    int signatureBits = 0;
    int algorithm = 0;
    LibreSsl3ValidityX509 certificate {};
    const void* signature = &Before;
    const void* signatureAlgorithm = &Before;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, &certificate);
    Require(signature == nullptr && signatureAlgorithm == nullptr);
    certificate.signature = &signatureBits;
    certificate.sigAlg = &algorithm;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, &certificate);
    Require(signature == &signatureBits && signatureAlgorithm == &algorithm);
    signature = signatureAlgorithm = &After;
    X509_get0_signature_nid_postfix(nullptr, &signatureAlgorithm, &certificate);
    Require(signature == &After && signatureAlgorithm == &algorithm);
    X509_get0_signature_nid_postfix(&signature, nullptr, &certificate);
    Require(signature == &signatureBits);
    X509_get0_signature_nid_postfix(nullptr, nullptr, &certificate);
    X509_get0_signature_nid_postfix(nullptr, nullptr, nullptr);
    RequireThrows([&] { X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, nullptr); });
    RequireThrows([&] { X509_get0_signature_nid_postfix(&signature, nullptr, nullptr); });
    RequireThrows([&] { X509_get0_signature_nid_postfix(nullptr, &signatureAlgorithm, nullptr); });
}

int main() {
    TestGetmNotBefore();
    TestGetmNotAfter();
    TestGet0NotBefore();
    TestGet0NotAfter();
    TestGet0Signature();
    return 0;
}
