#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_MD_block_size check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    Require(EVP_MD_block_size_nid_postfix(EVP_md5_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha256_nid_postfix()) == 64);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha384_nid_postfix()) == 128);
    Require(EVP_MD_block_size_nid_postfix(EVP_sha512_nid_postfix()) == 128);
    EvpMd custom{};
    custom.mdSize = 20;
    custom.blockSize = 72;
    custom.contextSize = 96;
    Require(EVP_MD_block_size_nid_postfix(&custom) == 72);
}
