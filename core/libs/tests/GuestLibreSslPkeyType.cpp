#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_PKEY_type check failed at line %u\n", location.line());
    std::abort();
}

int main() {
    Require(EVP_PKEY_type_nid_postfix(6) == 6);
    Require(EVP_PKEY_type_nid_postfix(19) == 6);
    Require(EVP_PKEY_type_nid_postfix(408) == 408);
    Require(EVP_PKEY_type_nid_postfix(855) == 855);
    Require(EVP_PKEY_type_nid_postfix(894) == 894);
    Require(EVP_PKEY_type_nid_postfix(28) == 28);
    for (const int type : {66, 67, 70, 113, 116}) Require(EVP_PKEY_type_nid_postfix(type) == 116);
    for (const int type : {0, -1, 1, 5, 7, 64, 65, 117, 407, 409, 672, 854, 856, 811, 815, 893, 895, 946, 947, 948, INT_MIN, INT_MAX}) Require(EVP_PKEY_type_nid_postfix(type) == 0);
    std::puts("libSceLibreSsl EVP_PKEY_type tests passed");
}
