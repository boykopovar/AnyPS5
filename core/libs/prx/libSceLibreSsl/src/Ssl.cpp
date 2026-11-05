#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslSslInternal) == 432);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(sizeof(LibreSslSsl) == 232);
static_assert(offsetof(LibreSslSsl, rbio) == 16);
static_assert(offsetof(LibreSslSsl, wbio) == 24);
static_assert(offsetof(LibreSslSsl, session) == 128);
static_assert(offsetof(LibreSslSsl, ctx) == 152);
static_assert(offsetof(LibreSslSsl, verifyResult) == 160);
static_assert(offsetof(LibreSslSsl, internal) == 224);

namespace {

const LibreSslSsl& Connection(const LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslSslInternal& Internal(const LibreSslSsl* ssl, const char* function) {
    const LibreSslSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

}

extern "C" {

void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).ctx;
}

void* APS5_VABI SSL_get_rbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).rbio;
}

void* APS5_VABI SSL_get_wbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).wbio;
}

int APS5_VABI SSL_version_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).version;
}

std::int64_t APS5_VABI SSL_get_verify_result_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).verifyResult;
}

int APS5_VABI SSL_get_shutdown_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).shutdown;
}

int APS5_VABI SSL_want_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).rwstate;
}

}
