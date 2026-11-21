#include "prx/libSceLibreSsl3/include/Api.hpp"

extern "C" int APS5_VABI SSL_CTX_set0_chain_nid_postfix(LibreSsl3SslCtx* context, LibreSslStack* chain) {
    if (!context) return 0;
    context->extraCerts = chain;
    return 1;
}
