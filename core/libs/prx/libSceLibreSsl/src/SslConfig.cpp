#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <stdexcept>

namespace {

constexpr int HostNameType = 0;

}

extern "C" {

LibreSslX509VerifyParam* APS5_VABI SSL_get0_param_nid_postfix(LibreSslSsl* ssl) {
    if (!ssl) APS5_INVALID_ARG_EX;
    return static_cast<LibreSslX509VerifyParam*>(ssl->param);
}

const char* APS5_VABI SSL_get_servername_nid_postfix(const LibreSslSsl* ssl, int type) {
    if (!ssl) APS5_INVALID_ARG_EX;
    if (type != HostNameType) return nullptr;
    return ssl->tlsextHostname;
}

int APS5_VABI SSL_CTX_set_alpn_protos_nid_postfix(LibreSslCtx* ctx, const unsigned char* protos,
    unsigned int protosLength) {
    if (!ctx) APS5_INVALID_ARG_EX;
    if (!ctx->internal) throw std::invalid_argument("SSL_CTX_set_alpn_protos: null context internals");
    LibreSslCtxInternal& internal = *ctx->internal;
    if (!protos || protosLength == 0) return 1;
    auto* copy = static_cast<unsigned char*>(malloc_nid_postfix(protosLength));
    if (!copy) return 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(copy, protos, protosLength);
    errno = savedErrno;
    if (!copied) {
        free_nid_postfix(copy);
        return 1;
    }
    free_nid_postfix(internal.alpnClientProtoList);
    internal.alpnClientProtoList = copy;
    internal.alpnClientProtoListLen = protosLength;
    return 0;
}

}
