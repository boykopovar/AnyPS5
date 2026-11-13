#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EVP_PKEY_CTX_ctrl check failed at line %u\n", location.line());
    std::abort();
}

PkeyContext* NewContext() {
    void* pkey = EVP_PKEY_new_nid_postfix();
    auto* rsa = RSA_new_nid_postfix();
    Require(pkey != nullptr && rsa != nullptr);
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa) == 1);
    PkeyContext* context = PkeyContextNew_nid_no_patch(pkey, nullptr);
    EVP_PKEY_free_nid_postfix(pkey);
    Require(context != nullptr);
    return context;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    PkeyContext* context = NewContext();
    context->operation = 1;
    const EvpMd* digest = EVP_sha256_nid_postfix();
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, -1, 1, 1, 0, const_cast<EvpMd*>(digest)) == 1);
    Require(static_cast<RsaPkeyContext*>(context->data)->digest == digest);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, 408, 1, 1, 0, const_cast<EvpMd*>(digest)) == -1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(nullptr, -1, 1, 1, 0, nullptr) == -2);
    PkeyContextFree_nid_no_patch(context);

    std::puts("EVP_PKEY_CTX_ctrl tests passed");
}
