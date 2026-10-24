#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

extern "C" const char* APS5_VABI X509_verify_cert_error_string_nid_postfix(std::int64_t);

static constexpr std::uint64_t ExpectedHashes[] {
    0x8b05d07b5566befULL, 0, 0x708834e4234778b5ULL, 0xefb3526cbf8b4d8fULL,
    0xf9c246e135cdbad1ULL, 0xd42f63e9d35f4aa1ULL, 0xcccbec8974d6af08ULL, 0x8d443856acab19a4ULL,
    0x8d2b572b67e1760ULL, 0xa3b575a1199d9fd1ULL, 0xbed7043d1daf8f09ULL, 0x2a0a7832fb23e29dULL,
    0x265d14ebfd73f005ULL, 0x675282b2d8705cd0ULL, 0xade734fba45db83ULL, 0x2688e741036ec90fULL,
    0xf75897f162ad286eULL, 0x74e217b21ed87663ULL, 0xa33689775feaef0eULL, 0xa3359b0eaed11517ULL,
    0x6925fd54e311eb7cULL, 0xe6f75276f756e2e2ULL, 0xc9122d9145ee8c0fULL, 0xe88507a36fdaa6e6ULL,
    0xe4225a4c8853b86fULL, 0x5ef1c83b11d335a6ULL, 0x9b6dfad0758a0221ULL, 0xfe7f0ff3aadd086ULL,
    0x85a68d2a11031f82ULL, 0x72c1d177130ec956ULL, 0x44d8d6b5ed2f06edULL, 0x738e7dc7a61e12cdULL,
    0xc1fe96db71c85021ULL, 0x26559c0cfe2f2f78ULL, 0xc3bf2a493ebacd94ULL, 0xa4206ad9f25787ddULL,
    0x3f7344fff3aa908bULL, 0x23335a5daca94f1aULL, 0x1d9d2747a36c85e2ULL, 0xf07fa5bffdd43063ULL,
    0x1bad38ca7c7bc1e8ULL, 0x7715922cd76c9dceULL, 0x82fcbf1e3440e64eULL, 0x6d1f21d68462a5d4ULL,
    0x2e572ca0e246a7c9ULL, 0x5e23e31a525a543bULL, 0xa0bd09ebfc8cc89ULL, 0x85288e1dd50bdaceULL,
    0xf659605c19b0997eULL, 0xddad96c31c47ffdeULL, 0x2ccb58faf48aae6aULL, 0x9973a358685fe222ULL,
    0xe154c3e01bf7ba9bULL, 0x1e042e731311c0e8ULL, 0xa9af33b6e407c014ULL,
};

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

static std::uint64_t Hash(const char* text) {
    Require(text != nullptr, "error string is non-null");
    std::uint64_t value = 14695981039346656037ULL;
    for (; *text; ++text) value = (value ^ static_cast<unsigned char>(*text)) * 1099511628211ULL;
    return value;
}

int main() {
    constexpr std::int64_t offsets[] {0, INT64_C(0x100000000), -INT64_C(0x100000000),
                                    std::numeric_limits<std::int64_t>::min(), INT64_C(0x7fffffff00000000)};
    for (std::int64_t code = 0; code < 55; ++code) {
        if (code == 1) continue;
        const char* stable = X509_verify_cert_error_string_nid_postfix(code);
        for (const auto offset : offsets) {
            const char* actual = X509_verify_cert_error_string_nid_postfix(code + offset);
            Require(Hash(actual) == ExpectedHashes[code], "complete LibreSSL 2.5.4 message mapping");
            Require(actual == stable, "known messages retain static storage across 64-bit aliases");
        }
    }

    for (std::int64_t code = -300; code <= 300; ++code) {
        if (code == 0 || (code >= 2 && code <= 54)) continue;
        const std::string expected = "error number " + std::to_string(code);
        const char* actual = X509_verify_cert_error_string_nid_postfix(code);
        Require(actual && expected == actual, "unknown and newer codes retain the numeric fallback");
    }

    struct WideCase { std::int64_t error; const char* expected; };
    constexpr WideCase wideCases[] {
        {std::numeric_limits<std::int64_t>::max(), "error number 9223372036854775807"},
        {std::numeric_limits<std::int64_t>::min() + 1, "error number -9223372036854775807"},
        {INT64_C(4294967297), "error number 4294967297"},
        {-INT64_C(4294967295), "error number -4294967295"},
        {INT64_C(2147483648), "error number 2147483648"},
        {-INT64_C(2147483648), "error number -2147483648"},
        {INT64_C(4294967358), "error number 4294967358"},
    };
    for (const auto& item : wideCases) {
        const char* actual = X509_verify_cert_error_string_nid_postfix(item.error);
        Require(actual && std::strcmp(actual, item.expected) == 0, "fallback formats all 64 argument bits");
    }

    const char* shared = X509_verify_cert_error_string_nid_postfix(-123);
    const char* known = X509_verify_cert_error_string_nid_postfix(10);
    Require(std::strcmp(shared, "error number -123") == 0, "known lookup preserves unknown buffer");
    const char* next = X509_verify_cert_error_string_nid_postfix(62);
    Require(shared == next && std::strcmp(shared, "error number 62") == 0, "unknown results share overwritten storage");
    Require(std::strcmp(known, "certificate has expired") == 0, "unknown formatting preserves known message");
    Require(std::strcmp(X509_verify_cert_error_string_nid_postfix(0), "ok") == 0, "success message");
    return 0;
}
