#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX_set0_chain check failed at line %u\n", location.line());
    std::abort();
}
}
int main() {
    LibreSsl3SslCtx context{};
    LibreSslStack stack{};
    Require(SSL_CTX_set0_chain_nid_postfix(&context, &stack) == 1);
    Require(context.extraCerts == &stack);
    Require(SSL_CTX_set0_chain_nid_postfix(nullptr, &stack) == 0);
    std::puts("libSceLibreSsl3 SSL_CTX_set0_chain tests passed");
}
