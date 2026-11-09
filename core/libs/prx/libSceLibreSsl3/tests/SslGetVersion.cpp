#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <cstring>
#include <source_location>
#include <cstdlib>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_get_version check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3Ssl ssl{};
    ssl.version = 0x0303;
    Require(std::strcmp(SSL_get_version_nid_postfix(&ssl), "TLSv1.2") == 0);
    ssl.version = 0x0304;
    Require(std::strcmp(SSL_get_version_nid_postfix(&ssl), "TLSv1.3") == 0);
    ssl.version = 0x0300;
    Require(std::strcmp(SSL_get_version_nid_postfix(&ssl), "SSLv3") == 0);
    ssl.version = 0x1234;
    Require(std::strcmp(SSL_get_version_nid_postfix(&ssl), "unknown") == 0);
    bool threw = false;
    try { SSL_get_version_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    std::puts("libSceLibreSsl3 SSL_get_version tests passed");
}
