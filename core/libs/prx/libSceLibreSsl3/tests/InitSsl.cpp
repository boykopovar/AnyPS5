#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 OPENSSL_init_ssl check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t NoLoadConfig = 0x1;
constexpr std::uint64_t LoadConfig = 0x2;
constexpr std::uint64_t NoOperation = 0x80000000;

}

int main() {
    Require(OPENSSL_init_ssl_nid_postfix(0, nullptr) == 1);
    Require(OPENSSL_init_ssl_nid_postfix(NoOperation, nullptr) == 1);
    Require(OPENSSL_init_ssl_nid_postfix(NoLoadConfig | LoadConfig, nullptr) == 1);
    Require(OPENSSL_init_ssl_nid_postfix(LoadConfig, nullptr) == 1);
    Require(OPENSSL_init_ssl_nid_postfix(NoLoadConfig, nullptr) == 1);
    Require(OPENSSL_init_ssl_nid_postfix(0, nullptr) == 1);
}
