#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_VERIFY_PARAM check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t ExplicitPolicy = 0x100;
constexpr std::uint64_t InhibitAny = 0x200;
constexpr std::uint64_t InhibitMap = 0x400;
constexpr std::uint64_t PartialChain = 0x80000;

struct GuardedParam {
    std::array<std::uint64_t, 2> before;
    LibreSslX509VerifyParam param;
    std::array<std::uint64_t, 2> after;
};

void CheckSetFlags(std::uint64_t initial, std::uint64_t flags) {
    GuardedParam guarded;
    std::memset(&guarded, 0x5A, sizeof(guarded));
    guarded.param.flags = initial;
    GuardedParam expected;
    std::memcpy(&expected, &guarded, sizeof(guarded));
    expected.param.flags = initial | flags;
    Require(X509_VERIFY_PARAM_set_flags_nid_postfix(&guarded.param, flags) == 1);
    Require(std::memcmp(&guarded, &expected, sizeof(guarded)) == 0);
}

void CheckVersionDependent(std::uint64_t initial, std::uint64_t flags) {
    GuardedParam guarded;
    std::memset(&guarded, 0x5A, sizeof(guarded));
    guarded.param.flags = initial;
    GuardedParam expected;
    std::memcpy(&expected, &guarded, sizeof(guarded));
    bool thrown = false;
    try {
        X509_VERIFY_PARAM_set_flags_nid_postfix(&guarded.param, flags);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(std::memcmp(&guarded, &expected, sizeof(guarded)) == 0);
}

void TestSetFlags() {
    CheckSetFlags(0, 0);
    CheckSetFlags(0, PartialChain);
    CheckSetFlags(PartialChain, PartialChain);
    CheckSetFlags(0x8000, 0x4);
    CheckSetFlags(~std::uint64_t {0}, 0x1);
    CheckSetFlags(0x1, ~std::uint64_t {0});
    CheckSetFlags(0, PolicyCheck);
    CheckSetFlags(0, PolicyCheck | ExplicitPolicy);
    CheckSetFlags(PolicyCheck, ExplicitPolicy);
    CheckSetFlags(PolicyCheck | PartialChain, InhibitAny | InhibitMap);
    for (std::uint64_t initial : {std::uint64_t {0}, PartialChain, ExplicitPolicy})
        for (std::uint64_t flags : {ExplicitPolicy, InhibitAny, InhibitMap, ExplicitPolicy | PartialChain})
            CheckVersionDependent(initial, flags);
    bool thrown = false;
    try {
        X509_VERIFY_PARAM_set_flags_nid_postfix(nullptr, PartialChain);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

}

int main() {
    TestSetFlags();
    std::puts("LibreSSL3 X509_VERIFY_PARAM tests passed");
}
