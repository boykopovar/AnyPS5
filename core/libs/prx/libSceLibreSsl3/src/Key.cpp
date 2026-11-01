#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"

#include <stdexcept>

extern "C" {

int APS5_VABI EVP_PKEY_up_ref_nid_postfix(LibreSsl3EvpPkey* key) {
    if (!key)
        throw std::invalid_argument("EVP_PKEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI EC_KEY_up_ref_nid_postfix(LibreSsl3EcKey* key) {
    if (!key)
        throw std::invalid_argument("EC_KEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI RSA_up_ref_nid_postfix(LibreSsl3Rsa* rsa) {
    if (!rsa)
        throw std::invalid_argument("RSA_up_ref requires a key");
    return Retain_nid_no_patch(rsa->references);
}

int APS5_VABI SSL_CTX_up_ref_nid_postfix(LibreSsl3SslCtx* ctx) {
    if (!ctx)
        throw std::invalid_argument("SSL_CTX_up_ref requires a context");
    return Retain_nid_no_patch(ctx->references);
}

}
