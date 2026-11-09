#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_set_accept_state check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3Ssl ssl{};
    ssl.server = 0;
    SSL_set_accept_state_nid_postfix(&ssl);
    Require(ssl.server == 1);
    bool threw = false;
    try { SSL_set_accept_state_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    std::puts("libSceLibreSsl3 SSL_set_accept_state tests passed");
}
