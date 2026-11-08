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
    Asn1Object_nid_no_patch firstObject{};
    firstObject.nid = 0;
    firstObject.length = 3;
    firstObject.data = basicConstraints;
    Asn1Object_nid_no_patch secondObject{};
    secondObject.nid = 83;
    Extension first{&firstObject, 0, 0, nullptr};
    Extension second{&secondObject, 1, 0, nullptr};
    std::array<char*, 2> entries{reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second)};
    LibreSsl3Stack stack{2, entries.data(), 0, 2, nullptr};
    LibreSsl3ExtensionsCertInfo certificateInfo{};
    certificateInfo.extensions = &stack;
    LibreSsl3ExtensionsX509 certificate{};
    certificate.certInfo = &certificateInfo;

    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 87, -1) == 0);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 87, 0) == -1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 83, -1) == 1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 83, 0) == 1);
    Require(X509_get_ext_by_NID_nid_postfix(&certificate, 1000000, -1) == -2);
    Require(X509_get_ext_by_NID_nid_postfix(nullptr, 87, -1) == -1);

    LibreSsl3ExtensionsCertInfo emptyInfo{};
    LibreSsl3ExtensionsX509 emptyCertificate{};
    emptyCertificate.certInfo = &emptyInfo;
    Require(X509_get_ext_by_NID_nid_postfix(&emptyCertificate, 87, -1) == -1);

    std::puts("libSceLibreSsl3 X509 extension lookup tests passed");
}
