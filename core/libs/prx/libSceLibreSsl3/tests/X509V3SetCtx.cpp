#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509V3_set_ctx check failed at line %u\n", location.line());
    std::abort();
}
struct Context { int flags; int padding; void* issuer; void* subject; void* request; void* crl; void* databaseMethod; void* database; };
static_assert(sizeof(Context) == 56);
}

int main() {
    Context context{};
    int issuer = 1, subject = 2, request = 3, crl = 4;
    X509V3_set_ctx_nid_postfix(&context, &issuer, &subject, &request, &crl, 0x1);
    Require(context.flags ==  0x1 && context.issuer == &issuer && context.subject == &subject);
    Require(context.request == &request && context.crl == &crl);
    bool threw = false;
    try { X509V3_set_ctx_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr, 0); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    std::puts("libSceLibreSsl3 X509V3_set_ctx tests passed");
}
