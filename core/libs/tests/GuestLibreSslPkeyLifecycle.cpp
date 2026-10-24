#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>

extern "C" {
void* APS5_VABI EVP_PKEY_new_nid_postfix(void);
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
    void* other = EVP_PKEY_new_nid_postfix();
    Require(other != nullptr && other != pkey, "each call returns a distinct key");
    return 0;
}
