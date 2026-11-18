#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509 extension check failed at line %u\n", location.line());
    std::abort();
}

static void TestGet0Extensions() {
    LibreSsl3Stack extensions {};
    LibreSsl3ExtensionsCertInfo info {};
    LibreSsl3ExtensionsX509 certificate {};
    certificate.certInfo = &info;
    Require(X509_get0_extensions_nid_postfix(&certificate) == nullptr);
    std::memset(info.beforeExtensions, 0xFF, sizeof(info.beforeExtensions));
    std::memset(info.rest, 0xFF, sizeof(info.rest));
    info.extensions = &extensions;
    Require(X509_get0_extensions_nid_postfix(&certificate) == &extensions);
}

static void TestGetExtCount() {
    LibreSsl3Stack extensions {};
    LibreSsl3ExtensionsCertInfo info {};
    LibreSsl3ExtensionsX509 certificate {};
    certificate.certInfo = &info;
    Require(X509_get_ext_count_nid_postfix(&certificate) == 0);
    info.extensions = &extensions;
    Require(X509_get_ext_count_nid_postfix(&certificate) == 0);
    extensions.num = 3;
    extensions.numAlloc = 8;
    Require(X509_get_ext_count_nid_postfix(&certificate) == 3);
}

static void TestGetExt() {
    int first = 0;
    int second = 0;
    int third = 0;
    char* slots[] {reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second), reinterpret_cast<char*>(&third)};
    LibreSsl3Stack extensions {};
    LibreSsl3ExtensionsCertInfo info {};
    LibreSsl3ExtensionsX509 certificate {};
    certificate.certInfo = &info;
    Require(X509_get_ext_nid_postfix(&certificate, 0) == nullptr);
    info.extensions = &extensions;
    Require(X509_get_ext_nid_postfix(&certificate, 0) == nullptr);
    extensions = {2, slots, 0, 3, nullptr};
    Require(X509_get_ext_nid_postfix(&certificate, 0) == &first);
    Require(X509_get_ext_nid_postfix(&certificate, 1) == &second);
    Require(X509_get_ext_nid_postfix(&certificate, 2) == nullptr);
    Require(X509_get_ext_nid_postfix(&certificate, -1) == nullptr);
    Require(X509_get_ext_nid_postfix(&certificate, INT_MIN) == nullptr);
    Require(X509_get_ext_nid_postfix(&certificate, INT_MAX) == nullptr);
    extensions.num = 3;
    Require(X509_get_ext_nid_postfix(&certificate, 2) == &third);
    Require(X509_get_ext_nid_postfix(&certificate, 3) == nullptr);
}

int main() {
    TestGet0Extensions();
    TestGetExtCount();
    TestGetExt();
    return 0;
}
