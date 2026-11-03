#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509V3_set_ctx check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    LibreSslX509 issuer{};
    LibreSslX509 subject{};
    int request = 0;
    int crl = 0;
    int database = 0;
    LibreSsl3X509V3Context context{};
    context.database = &database;
    X509V3_set_ctx_nid_postfix(&context, &issuer, &subject, &request, &crl, 1);
    Require(context.flags == 1);
    Require(context.issuerCertificate == &issuer && context.subjectCertificate == &subject);
    Require(context.subjectRequest == &request && context.crl == &crl);
    Require(context.database == &database);
    X509V3_set_ctx_nid_postfix(&context, nullptr, &subject, nullptr, nullptr, 0);
    Require(context.flags == 0);
    Require(context.issuerCertificate == nullptr && context.subjectCertificate == &subject);
    Require(context.subjectRequest == nullptr && context.crl == nullptr);
    Require(context.database == &database);
}
