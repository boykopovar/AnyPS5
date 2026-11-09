#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

namespace {

constexpr std::int32_t EvpPkeyRsa = 6;
constexpr std::int32_t EvpPkeyDh = 28;
constexpr std::int32_t EvpPkeyDsa = 116;
constexpr std::int32_t EvpPkeyEc = 408;
constexpr std::int32_t EvpPkeyRsaPss = 912;
constexpr std::int32_t EvpPkeyUnknown = 9999;

constexpr int EvpRExpectingAnRsaKey = 127;
constexpr int EvpRExpectingADhKey = 128;
constexpr int EvpRExpectingADsaKey = 129;
constexpr int EvpRExpectingAnEcKey = 142;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY getter check failed at line %u\n", location.line());
    std::abort();
}

LibreSsl3EvpPkey KeyWith(std::int32_t type, void* inner) {
    LibreSsl3EvpPkey key{};
    key.type = type;
    key.references = 1;
    key.key = inner;
    return key;
}

constexpr std::uint64_t EvpError(int reason) {
    return (6ULL << 24) | (0xfffULL << 12) | static_cast<std::uint64_t>(reason & 0xfff);
}

void CheckError(std::uint64_t code, int expectedReason, std::source_location location = std::source_location::current()) {
    Require(code == EvpError(expectedReason), location);
    Require(((code >> 24) & 0xff) == 6, location);
    Require(((code >> 12) & 0xfff) == 0xfff, location);
    Require((code & 0xfff) == static_cast<std::uint64_t>(expectedReason), location);
}

void RunTests() {
    LibreSsl3Rsa rsa{};
    int dsa = 0;
    int dh = 0;
    LibreSsl3EcKey ec{};
    int unknown = 0;

    auto rsaKey = KeyWith(EvpPkeyRsa, &rsa);
    auto rsaPssKey = KeyWith(EvpPkeyRsaPss, &rsa);
    auto dsaKey = KeyWith(EvpPkeyDsa, &dsa);
    auto dhKey = KeyWith(EvpPkeyDh, &dh);
    auto ecKey = KeyWith(EvpPkeyEc, &ec);
    auto unknownKey = KeyWith(EvpPkeyUnknown, &unknown);

    errno = 123;
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaKey) == &rsa);
    Require(errno == 123);
    Require(rsaKey.references == 1 && rsaKey.key == &rsa);

    errno = 124;
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaPssKey) == &rsa);
    Require(errno == 124);
    Require(rsaPssKey.references == 1 && rsaPssKey.key == &rsa);

    errno = 125;
    Require(EVP_PKEY_get0_DSA_nid_postfix(&dsaKey) == &dsa);
    Require(errno == 125);
    Require(dsaKey.references == 1 && dsaKey.key == &dsa);

    errno = 126;
    Require(EVP_PKEY_get0_DH_nid_postfix(&dhKey) == &dh);
    Require(errno == 126);
    Require(dhKey.references == 1 && dhKey.key == &dh);

    errno = 127;
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&ecKey) == &ec);
    Require(errno == 127);
    Require(ecKey.references == 1 && ecKey.key == &ec);

    auto emptyRsa = KeyWith(EvpPkeyRsa, nullptr);
    auto emptyPss = KeyWith(EvpPkeyRsaPss, nullptr);
    auto emptyDsa = KeyWith(EvpPkeyDsa, nullptr);
    auto emptyDh = KeyWith(EvpPkeyDh, nullptr);

    errno = 201;
    Require(EVP_PKEY_get0_RSA_nid_postfix(&emptyRsa) == nullptr);
    Require(errno == 201);
    Require(emptyRsa.references == 1 && emptyRsa.key == nullptr);
    Require(ERR_peek_last_error_nid_postfix() == 0);

    errno = 202;
    Require(EVP_PKEY_get0_RSA_nid_postfix(&emptyPss) == nullptr);
    Require(errno == 202);
    Require(emptyPss.references == 1 && emptyPss.key == nullptr);
    Require(ERR_peek_last_error_nid_postfix() == 0);

    errno = 203;
    Require(EVP_PKEY_get0_DSA_nid_postfix(&emptyDsa) == nullptr);
    Require(errno == 203);
    Require(emptyDsa.references == 1 && emptyDsa.key == nullptr);
    Require(ERR_peek_last_error_nid_postfix() == 0);

    errno = 204;
    Require(EVP_PKEY_get0_DH_nid_postfix(&emptyDh) == nullptr);
    Require(errno == 204);
    Require(emptyDh.references == 1 && emptyDh.key == nullptr);
    Require(ERR_peek_last_error_nid_postfix() == 0);

    auto emptyEc = KeyWith(EvpPkeyEc, nullptr);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&emptyEc) == nullptr);
    Require(emptyEc.references == 1 && emptyEc.key == nullptr);

    LibreSsl3EvpPkey* rsaMismatches[] = {&dsaKey, &dhKey, &ecKey, &unknownKey};
    for (auto* mismatchKey : rsaMismatches) {
        const auto originalType = mismatchKey->type;
        const auto* originalInner = mismatchKey->key;
        errno = 301;
        Require(EVP_PKEY_get0_RSA_nid_postfix(mismatchKey) == nullptr);
        Require(errno == 301);
        Require(mismatchKey->references == 1);
        Require(mismatchKey->type == originalType);
        Require(mismatchKey->key == originalInner);
        CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnRsaKey);
    }

    errno = 401;
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaKey) == &rsa);
    Require(errno == 401);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnRsaKey);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&emptyRsa) == nullptr);
    Require(errno == 401);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnRsaKey);

    LibreSsl3EvpPkey* dsaMismatches[] = {&rsaKey, &rsaPssKey, &dhKey, &ecKey, &unknownKey};
    for (auto* mismatchKey : dsaMismatches) {
        const auto originalType = mismatchKey->type;
        const auto* originalInner = mismatchKey->key;
        errno = 501;
        Require(EVP_PKEY_get0_DSA_nid_postfix(mismatchKey) == nullptr);
        Require(errno == 501);
        Require(mismatchKey->references == 1);
        Require(mismatchKey->type == originalType);
        Require(mismatchKey->key == originalInner);
        CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADsaKey);
    }

    errno = 601;
    Require(EVP_PKEY_get0_DSA_nid_postfix(&dsaKey) == &dsa);
    Require(errno == 601);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADsaKey);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&emptyDsa) == nullptr);
    Require(errno == 601);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADsaKey);

    LibreSsl3EvpPkey* dhMismatches[] = {&rsaKey, &rsaPssKey, &dsaKey, &ecKey, &unknownKey};
    for (auto* mismatchKey : dhMismatches) {
        const auto originalType = mismatchKey->type;
        const auto* originalInner = mismatchKey->key;
        errno = 701;
        Require(EVP_PKEY_get0_DH_nid_postfix(mismatchKey) == nullptr);
        Require(errno == 701);
        Require(mismatchKey->references == 1);
        Require(mismatchKey->type == originalType);
        Require(mismatchKey->key == originalInner);
        CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADhKey);
    }

    errno = 801;
    Require(EVP_PKEY_get0_DH_nid_postfix(&dhKey) == &dh);
    Require(errno == 801);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADhKey);
    Require(EVP_PKEY_get0_DH_nid_postfix(&emptyDh) == nullptr);
    Require(errno == 801);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingADhKey);

    LibreSsl3EvpPkey* ecMismatches[] = {&rsaKey, &rsaPssKey, &dsaKey, &dhKey, &unknownKey};
    for (auto* mismatchKey : ecMismatches) {
        const auto originalType = mismatchKey->type;
        const auto* originalInner = mismatchKey->key;
        errno = 901;
        Require(EVP_PKEY_get0_EC_KEY_nid_postfix(mismatchKey) == nullptr);
        Require(errno == 901);
        Require(mismatchKey->references == 1);
        Require(mismatchKey->type == originalType);
        Require(mismatchKey->key == originalInner);
        CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnEcKey);
    }

    errno = 902;
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&ecKey) == &ec);
    Require(errno == 902);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnEcKey);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&emptyEc) == nullptr);
    Require(errno == 902);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnEcKey);

    const auto mainThreadLast = ERR_peek_last_error_nid_postfix();
    std::thread worker([&]() {
        Require(ERR_peek_last_error_nid_postfix() == 0);
        auto workerKey = KeyWith(EvpPkeyDsa, &dsa);
        Require(EVP_PKEY_get0_RSA_nid_postfix(&workerKey) == nullptr);
        CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnRsaKey);
    });
    worker.join();
    Require(ERR_peek_last_error_nid_postfix() == mainThreadLast);

    ERR_clear_error_nid_postfix();
    Require(EVP_PKEY_get0_RSA_nid_postfix(&dsaKey) == nullptr);
    Require(EVP_PKEY_get0_DH_nid_postfix(&rsaKey) == nullptr);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&rsaKey) == nullptr);
    Require(EVP_PKEY_get0_RSA_nid_postfix(&rsaKey) == &rsa);
    Require(EVP_PKEY_get0_DSA_nid_postfix(&emptyDsa) == nullptr);
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&dhKey) == nullptr);
    CheckError(ERR_peek_error_nid_postfix(), EvpRExpectingAnRsaKey);
    CheckError(ERR_peek_last_error_nid_postfix(), EvpRExpectingAnEcKey);
    CheckError(ERR_get_error_nid_postfix(), EvpRExpectingAnRsaKey);
    CheckError(ERR_get_error_nid_postfix(), EvpRExpectingADhKey);
    CheckError(ERR_get_error_nid_postfix(), EvpRExpectingADsaKey);
    CheckError(ERR_get_error_nid_postfix(), EvpRExpectingAnEcKey);
    Require(ERR_get_error_nid_postfix() == 0);
}

} // namespace

int main() {
    try {
        RunTests();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unexpected exception: %s\n", error.what());
        return 1;
    }
    return 0;
}
