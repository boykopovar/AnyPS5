#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

extern "C" {
int APS5_VABI EVP_PKEY_type_nid_postfix(int);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_PKEY_type check failed at line %u\n", location.line());
    std::abort();
}

static void RequireThrows(int type, std::source_location location = std::source_location::current()) {
    try { EVP_PKEY_type_nid_postfix(type); } catch (const std::exception&) { return; }
    Require(false, location);
}

int main() {
    Require(EVP_PKEY_type_nid_postfix(6) == 6);
    Require(EVP_PKEY_type_nid_postfix(19) == 6);
    Require(EVP_PKEY_type_nid_postfix(408) == 408);
    Require(EVP_PKEY_type_nid_postfix(855) == 855);
    Require(EVP_PKEY_type_nid_postfix(894) == 894);
    for (const int type : {28, 66, 67, 70, 113, 116, 811, 815, 946, 947}) RequireThrows(type);
    for (const int type : {0, -1, 1, 5, 7, 64, 65, 117, 407, 409, 672, 854, 856, 893, 895, 948, INT_MIN, INT_MAX}) Require(EVP_PKEY_type_nid_postfix(type) == 0);
    std::puts("libSceLibreSsl EVP_PKEY_type tests passed");
}
