#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>

extern "C" {
void* APS5_VABI EVP_PKEY_new_nid_postfix(void);
void APS5_VABI EVP_PKEY_free_nid_postfix(void* pkey);
}

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::terminate();
}
}

int main() {
    void* const api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    void* pkey = EVP_PKEY_new_nid_postfix();
    Require(pkey != nullptr, "allocates a key");
    int fields[3];
    std::memcpy(fields, pkey, sizeof(fields));
    Require(fields[0] == 0 && fields[1] == 0, "a new key has type and save_type EVP_PKEY_NONE");
    Require(fields[2] == 1, "a new key holds one reference");
    const int extra = 2;
    std::memcpy(static_cast<std::byte*>(pkey) + 8, &extra, sizeof(extra));
    EVP_PKEY_free_nid_postfix(pkey);
    std::memcpy(fields, pkey, sizeof(fields));
    Require(fields[2] == 1, "freeing a shared key only drops one reference");
    EVP_PKEY_free_nid_postfix(pkey);
    EVP_PKEY_free_nid_postfix(nullptr);
    void* withData = EVP_PKEY_new_nid_postfix();
    void* payload = &fields;
    std::memcpy(static_cast<std::byte*>(withData) + 32, &payload, sizeof(payload));
    try {
        EVP_PKEY_free_nid_postfix(withData);
    } catch (const std::exception&) {
        std::memcpy(fields, withData, sizeof(fields));
        Require(fields[2] == 1, "rejected free restores the last reference");
        void* noPayload = nullptr;
        std::memcpy(static_cast<std::byte*>(withData) + 32, &noPayload, sizeof(noPayload));
        EVP_PKEY_free_nid_postfix(withData);
        return 0;
    }
    Require(false, "a key holding algorithm data is rejected");
    return 1;
}
