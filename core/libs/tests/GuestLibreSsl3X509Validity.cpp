#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestValidity {
    void* notBefore;
    void* notAfter;
};

struct TestCertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    TestValidity* validity;
    void* subject;
    void* rest[7];
};

struct TestX509 {
    TestCertInfo* certInfo;
    const void* sigAlg;
    const void* signature;
    void* rest[24];
};

static_assert(sizeof(TestCertInfo) == 104 && offsetof(TestCertInfo, validity) == 32);
static_assert(sizeof(TestX509) == 216 && offsetof(TestX509, signature) == 16);

static int Before;
static int After;

static const LibreSsl3ValidityX509* Certificate(const TestX509* certificate) {
    return reinterpret_cast<const LibreSsl3ValidityX509*>(certificate);
}

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
    TestValidity validity {};
    TestCertInfo info {};
    TestX509 certificate {};
    Require(X509_getm_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notBefore_nid_postfix(Certificate(&certificate)) == &Before);
    validity.notBefore = nullptr;
    Require(X509_getm_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
}

static void TestGetmNotAfter() {
    TestValidity validity {};
    TestCertInfo info {};
    TestX509 certificate {};
    Require(X509_getm_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notAfter_nid_postfix(Certificate(&certificate)) == &After);
    validity.notAfter = nullptr;
    Require(X509_getm_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
}

static void TestGet0NotBefore() {
    TestValidity validity {};
    TestCertInfo info {};
    TestX509 certificate {};
    Require(X509_get0_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notBefore_nid_postfix(Certificate(&certificate)) == &Before);
    validity.notBefore = nullptr;
    Require(X509_get0_notBefore_nid_postfix(Certificate(&certificate)) == nullptr);
}

static void TestGet0NotAfter() {
    TestValidity validity {};
    TestCertInfo info {};
    TestX509 certificate {};
    Require(X509_get0_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notAfter_nid_postfix(Certificate(&certificate)) == &After);
    validity.notAfter = nullptr;
    Require(X509_get0_notAfter_nid_postfix(Certificate(&certificate)) == nullptr);
}

static void TestGet0Signature() {
    int signatureBits = 0;
    int algorithm = 0;
    TestX509 certificate {};
    const void* signature = &Before;
    const void* signatureAlgorithm = &Before;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, Certificate(&certificate));
    Require(signature == nullptr && signatureAlgorithm == nullptr);
    certificate.signature = &signatureBits;
    certificate.sigAlg = &algorithm;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, Certificate(&certificate));
    Require(signature == &signatureBits && signatureAlgorithm == &algorithm);
    signature = signatureAlgorithm = &After;
    X509_get0_signature_nid_postfix(nullptr, &signatureAlgorithm, Certificate(&certificate));
    Require(signature == &After && signatureAlgorithm == &algorithm);
    X509_get0_signature_nid_postfix(&signature, nullptr, Certificate(&certificate));
    Require(signature == &signatureBits);
    X509_get0_signature_nid_postfix(nullptr, nullptr, Certificate(&certificate));
    RequireThrows([&] { X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, nullptr); });
    RequireThrows([] { X509_get0_signature_nid_postfix(nullptr, nullptr, nullptr); });
}

int main() {
    TestGetmNotBefore();
    TestGetmNotAfter();
    TestGet0NotBefore();
    TestGet0NotAfter();
    TestGet0Signature();
    return 0;
}
