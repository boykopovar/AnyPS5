#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"

namespace {

LibreSsl3Bignum* Duplicate_nid_no_patch(const void* number) {
    return static_cast<LibreSsl3Bignum*>(static_cast<void*>(BN_dup_nid_postfix(static_cast<const Bignum*>(number))));
}

}

extern "C" {

LibreSsl3Rsa* APS5_VABI RSAPublicKey_dup_nid_postfix(const LibreSsl3Rsa* rsa) {
    if (!rsa || !rsa->n || !rsa->e) return nullptr;
    auto* copy = static_cast<LibreSsl3Rsa*>(calloc_nid_postfix(1, sizeof(LibreSsl3Rsa)));
    if (!copy) return nullptr;
    copy->version = 0;
    copy->references = 1;
    auto* n = Duplicate_nid_no_patch(rsa->n);
    if (!n) {
        free_nid_postfix(copy);
        return nullptr;
    }
    auto* e = Duplicate_nid_no_patch(rsa->e);
    if (!e) {
        BN_clear_free_nid_postfix(n);
        free_nid_postfix(copy);
        return nullptr;
    }
    if (!RSA_set0_key_nid_postfix(copy, n, e, nullptr)) {
        BN_clear_free_nid_postfix(n);
        BN_clear_free_nid_postfix(e);
        free_nid_postfix(copy);
        return nullptr;
    }
    return copy;
}

}
