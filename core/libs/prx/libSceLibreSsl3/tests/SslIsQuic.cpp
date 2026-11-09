#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_is_quic check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3Ssl ssl{};
    Require(SSL_is_quic_nid_postfix(&ssl) == 0);
    ssl.quicMethod = &ssl;
    Require(SSL_is_quic_nid_postfix(&ssl) == 1);
    Require(SSL_is_quic_nid_postfix(nullptr) == 0);
    std::puts("libSceLibreSsl3 SSL_is_quic tests passed");
}
