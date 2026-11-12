#include "prx/libSceLibreSsl3/include/Api.hpp"

#include <stdexcept>

extern "C" int APS5_VABI SSL_CTX_set_quic_method_nid_postfix(LibreSsl3SslCtx* context, const void* method) {
    if (!context || !context->method)
        throw std::invalid_argument("SSL_CTX_set_quic_method requires a context with a method");
    if (static_cast<const LibreSsl3SslMethod*>(context->method)->dtls) return 0;
    context->quicMethod = method;
    return 1;
}
