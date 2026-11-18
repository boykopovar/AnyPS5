#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 error-string loader check failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    const void* sha256 = EVP_get_digestbyname_nid_postfix("SHA256");
    Require(sha256 != nullptr);
    ERR_load_X509_strings_nid_postfix();
    ERR_load_PEM_strings_nid_postfix();
    OpenSSL_add_all_digests_nid_postfix();
    Require(EVP_get_digestbyname_nid_postfix("SHA256") == sha256);
    std::puts("libSceLibreSsl3 error-string loader tests passed");
}
