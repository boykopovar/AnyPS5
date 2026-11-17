#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
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

static const LibreSslX509* Certificate(const TestX509* certificate) {
    return reinterpret_cast<const LibreSslX509*>(certificate);
}

static void TestIssuerName() {
    int issuer = 0;
    int subject = 0;
    TestCertInfo info {};
    TestX509 certificate {};
    ExpectErr([] { X509_get_issuer_name_nid_postfix(nullptr); });
    ExpectErr([&] { X509_get_issuer_name_nid_postfix(Certificate(&certificate)); });
    certificate.certInfo = &info;
    Require(X509_get_issuer_name_nid_postfix(Certificate(&certificate)) == nullptr);
    info.issuer = &issuer;
    info.subject = &subject;
    Require(X509_get_issuer_name_nid_postfix(Certificate(&certificate)) == &issuer);
}

static void TestSubjectName() {
    int issuer = 0;
    int subject = 0;
    TestCertInfo info {};
    TestX509 certificate {};
    ExpectErr([] { X509_get_subject_name_nid_postfix(nullptr); });
    ExpectErr([&] { X509_get_subject_name_nid_postfix(Certificate(&certificate)); });
    certificate.certInfo = &info;
    Require(X509_get_subject_name_nid_postfix(Certificate(&certificate)) == nullptr);
    info.issuer = &issuer;
    info.subject = &subject;
    Require(X509_get_subject_name_nid_postfix(Certificate(&certificate)) == &subject);
}

static void TestSerialNumber() {
    int serial = 0;
    int version = 0;
    TestCertInfo info {};
    TestX509 certificate {};
    ExpectErr([] { X509_get_serialNumber_nid_postfix(nullptr); });
    ExpectErr([&] { X509_get_serialNumber_nid_postfix(Certificate(&certificate)); });
    certificate.certInfo = &info;
    Require(X509_get_serialNumber_nid_postfix(Certificate(&certificate)) == nullptr);
    info.version = &version;
    info.serialNumber = &serial;
    Require(X509_get_serialNumber_nid_postfix(Certificate(&certificate)) == &serial);
}

int main() {
    TestIssuerName();
    TestSubjectName();
    TestSerialNumber();
    return 0;
}
