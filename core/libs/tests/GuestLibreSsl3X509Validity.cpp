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

extern "C" {
void* APS5_VABI X509_getm_notBefore_nid_postfix(const TestX509*);
void* APS5_VABI X509_getm_notAfter_nid_postfix(const TestX509*);
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
    TestValidity validity {};
    TestCertInfo info {};
    TestX509 certificate {};
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

int main() {
    TestGetmNotBefore();
    TestGetmNotAfter();
    return 0;
}
