#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Reference.hpp"

extern "C" int APS5_VABI SSL_CTX_add1_chain_cert_nid_postfix(LibreSsl3SslCtx* context, LibreSsl3X509* certificate) {
    if (!context || !certificate) return 0;
    auto* stack = static_cast<LibreSslStack*>(context->extraCerts);
    if (!stack) {
        stack = sk_new_nid_postfix(nullptr);
        if (!stack) return 0;
        context->extraCerts = stack;
    }
    if (!X509_up_ref_nid_postfix(certificate)) return 0;
    if (sk_push_nid_postfix(stack, certificate)) return 1;
    Release_nid_no_patch(certificate->references);
    return 0;
}
