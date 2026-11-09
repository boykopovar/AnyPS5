#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 extension lookup check failed at line %u\n", location.line());
    std::abort();
}

struct Extension {
    const Asn1Object_nid_no_patch* object;
    int critical;
    int padding;
    const void* value;
};

static_assert(sizeof(Extension) == 24);

}

int main() {
    const unsigned char basicConstraints[] = {0x55, 0x1d, 0x13};
    const unsigned char keyUsage[] = {0x55, 0x1d, 0x0f};
    const unsigned char crlNumber[] = {0x55, 0x1d, 0x14};
    const unsigned char extendedKeyUsage[] = {0x55, 0x1d, 0x25};
    Asn1Object_nid_no_patch basicConstraintsObject{};
    basicConstraintsObject.nid = 0;
    basicConstraintsObject.length = 3;
    basicConstraintsObject.data = basicConstraints;
    Asn1Object_nid_no_patch keyUsageObject{};
    keyUsageObject.nid = 83;
    keyUsageObject.length = 3;
    keyUsageObject.data = keyUsage;
    Asn1Object_nid_no_patch crlNumberObject{};
    crlNumberObject.nid = 0;
    crlNumberObject.length = 3;
    crlNumberObject.data = crlNumber;
    Asn1Object_nid_no_patch extendedKeyUsageObject{};
    extendedKeyUsageObject.nid = 126;
    extendedKeyUsageObject.length = 3;
    extendedKeyUsageObject.data = extendedKeyUsage;
    Asn1Object_nid_no_patch mismatchedObject{};
    mismatchedObject.nid = 90;
    mismatchedObject.length = 3;
    mismatchedObject.data = crlNumber;
    Extension first{&basicConstraintsObject, 0, 0, nullptr};
    Extension second{&keyUsageObject, 1, 0, nullptr};
    Extension third{&crlNumberObject, 0, 0, nullptr};
    Extension fourth{&extendedKeyUsageObject, 0, 0, nullptr};
    Extension fifth{&mismatchedObject, 0, 0, nullptr};
    std::array<char*, 5> entries{reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second), reinterpret_cast<char*>(&third), reinterpret_cast<char*>(&fourth), reinterpret_cast<char*>(&fifth)};
    LibreSsl3Stack stack{5, entries.data(), 0, 5, nullptr};
    LibreSsl3ExtensionsCertInfo certificateInfo{};
    certificateInfo.extensions = &stack;
    LibreSsl3ExtensionsX509 certificate{};
    certificate.certInfo = &certificateInfo;

    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 87, -1) == 0);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 87, 0) == -1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 83, -1) == 1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 83, 0) == 1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 88, -1) == 2);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 88, 2) == 4);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 88, 4) == -1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 126, -1) == 3);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 90, -1) == -1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 1000000, -1) == -2);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, -1, -1) == -2);
    Require(X509_get_ext_by_NID_nid_postfix(nullptr, 87, -1) == -1);

    LibreSsl3ExtensionsCertInfo emptyInfo{};
    LibreSsl3ExtensionsX509 emptyCertificate{};
    emptyCertificate.certInfo = &emptyInfo;
    Require(X509_get_ext_by_NID_nid_postfix(&emptyCertificate, 87, -1) == -1);

    std::puts("libSceLibreSsl3 X509 extension lookup tests passed");
}
