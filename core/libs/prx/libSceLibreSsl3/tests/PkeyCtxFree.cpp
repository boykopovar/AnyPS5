#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EVP_PKEY_CTX_free check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    EVP_PKEY_CTX_free_nid_postfix(nullptr);

    void* pkey = EVP_PKEY_new_nid_postfix();
    auto* rsa = RSA_new_nid_postfix();
    Require(pkey != nullptr && rsa != nullptr);
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa) == 1);
    PkeyContext* context = PkeyContextNew_nid_no_patch(pkey, nullptr);
    Require(context != nullptr);
    EVP_PKEY_free_nid_postfix(pkey);
    EVP_PKEY_CTX_free_nid_postfix(context);

    std::puts("EVP_PKEY_CTX_free tests passed");
}
