#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

struct LibreSsl3Asn1String {
    int length;
    int type;
    unsigned char* data;
    std::int64_t flags;
};

struct LibreSsl3X509Algor {
    void* algorithm;
    void* parameter;
};

struct LibreSsl3X509Pubkey {
    LibreSsl3X509Algor* algor;
    LibreSsl3Asn1String* publicKey;
    void* pkey;
};

extern "C" {
int APS5_VABI X509_PUBKEY_get0_param_nid_postfix(void**, const unsigned char**, int*, void**, const void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 public-key parameter check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, location);
}

int main() {
    void* oid = reinterpret_cast<void*>(0x1234);
    void* parameter = reinterpret_cast<void*>(0x5678);
    unsigned char keyBytes[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    LibreSsl3Asn1String bitString{5, 3, keyBytes, 0};
    LibreSsl3X509Algor algorithmInfo{oid, parameter};
    LibreSsl3X509Pubkey pubkey{&algorithmInfo, &bitString, nullptr};

    void* returnedOid = nullptr;
    const unsigned char* returnedKey = nullptr;
    int returnedLength = -1;
    void* returnedAlgorithm = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, &returnedLength,
        &returnedAlgorithm, &pubkey) == 1);
    Require(returnedOid == oid && returnedKey == keyBytes && returnedLength == 5 && returnedAlgorithm == &algorithmInfo);

    returnedKey = nullptr;
    returnedLength = -1;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, &pubkey) == 1);
    Require(returnedKey == keyBytes && returnedLength == 5);

    returnedLength = 91;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, &returnedLength, nullptr, &pubkey) == 1);
    Require(returnedLength == 91);

    returnedOid = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, &pubkey) == 1);
    Require(returnedOid == oid);

    returnedAlgorithm = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, nullptr, &returnedAlgorithm, &pubkey) == 1);
    Require(returnedAlgorithm == &algorithmInfo);
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr) == 1);

    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, nullptr, nullptr, &pubkey); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, nullptr); });
    LibreSsl3X509Pubkey noAlgorithm{nullptr, &bitString, nullptr};
    LibreSsl3X509Pubkey noPublicKey{&algorithmInfo, nullptr, nullptr};
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, &noAlgorithm); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, &noPublicKey); });

    std::puts("libSceLibreSsl3 X509 public-key parameter tests passed");
}
