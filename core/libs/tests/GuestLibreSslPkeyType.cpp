#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_PKEY check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static std::uint64_t ErrorCode(int library, int function, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) |
           (static_cast<std::uint64_t>(function) << 12) |
           static_cast<std::uint64_t>(reason);
}

static void TestGet1EcKey() {
    constexpr int EcKeyType = 408;
    constexpr int RsaKeyType = 6;
    constexpr int ErrorFunction = 0xfff;
    constexpr int ExpectingEcKey = 142;
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    LibreSslPutError_nid_no_patch(4, 5, 6);
    RequireThrows([] { EVP_PKEY_get1_EC_KEY_nid_postfix(nullptr); });
    Require(ERR_peek_error_nid_postfix() == ErrorCode(1, 2, 3));
    int ecType = EcKeyType;
    unsigned char before[sizeof(ecType)];
    int rsaType = RsaKeyType;
    std::memcpy(before, &rsaType, sizeof(rsaType));
    Require(EVP_PKEY_get1_EC_KEY_nid_postfix(&rsaType) == nullptr);
    Require(std::memcmp(before, &rsaType, sizeof(rsaType)) == 0);
    Require(ERR_peek_error_nid_postfix() == ErrorCode(1, 2, 3));
    Require(ERR_get_error_nid_postfix() == ErrorCode(1, 2, 3));
    Require(ERR_get_error_nid_postfix() == ErrorCode(4, 5, 6));
    Require(ERR_get_error_nid_postfix() == ErrorCode(6, ErrorFunction, ExpectingEcKey));
    Require(ERR_get_error_nid_postfix() == 0);
}

int main() {
    TestGet1EcKey();
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
