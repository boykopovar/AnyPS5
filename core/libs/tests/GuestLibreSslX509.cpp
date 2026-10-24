#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestCertInfo {
    void* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    void* validity;
    void* subject;
    void* key;
    void* rest[6];
};

struct TestX509 {
    TestCertInfo* certInfo;
    void* rest[21];
};

static_assert(sizeof(TestCertInfo) == 104 && offsetof(TestCertInfo, issuer) == 24 && offsetof(TestCertInfo, subject) == 40);
static_assert(sizeof(TestX509) == 176);

extern "C" {
void* APS5_VABI X509_get_issuer_name_nid_postfix(const TestX509*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestIssuerName() {
    int issuer = 0;
    int subject = 0;
    TestCertInfo info {};
    TestX509 certificate {};
    RequireThrows([] { X509_get_issuer_name_nid_postfix(nullptr); });
    RequireThrows([&] { X509_get_issuer_name_nid_postfix(&certificate); });
    certificate.certInfo = &info;
    Require(X509_get_issuer_name_nid_postfix(&certificate) == nullptr);
    info.issuer = &issuer;
    info.subject = &subject;
    Require(X509_get_issuer_name_nid_postfix(&certificate) == &issuer);
}

int main() {
    TestIssuerName();
    return 0;
}
