#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_set_issuer_name check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    int name = 1;
    Require(X509_set_issuer_name_nid_postfix(&certificate, &name) == 1);
    Require(info.issuer == &name);
    Require(X509_set_issuer_name_nid_postfix(nullptr, &name) == 0);
    LibreSsl3Certificate empty{};
    Require(X509_set_issuer_name_nid_postfix(&empty, &name) == 0);
    std::puts("libSceLibreSsl3 X509_set_issuer_name tests passed");
}
