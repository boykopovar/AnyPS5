#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_MD_block_size check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    LibreSsl3::DigestMethod method{};
    method.blockSize = 64;
    Require(EVP_MD_block_size_nid_postfix(&method) == 64);
    method.blockSize = 128;
    Require(EVP_MD_block_size_nid_postfix(&method) == 128);
    Require(EVP_MD_block_size_nid_postfix(nullptr) == 0);
    std::puts("libSceLibreSsl3 EVP_MD_block_size tests passed");
}
