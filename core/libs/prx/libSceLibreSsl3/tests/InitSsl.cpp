#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

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
