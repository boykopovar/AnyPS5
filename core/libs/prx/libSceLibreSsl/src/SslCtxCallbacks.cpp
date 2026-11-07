#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxInternal) == 464);
static_assert(offsetof(LibreSslCtxInternal, newSessionCb) == 24);
static_assert(offsetof(LibreSslCtxInternal, appVerifyCallback) == 48);
static_assert(offsetof(LibreSslCtxInternal, appVerifyArg) == 56);
static_assert(offsetof(LibreSslCtxInternal, clientCertCb) == 64);
static_assert(offsetof(LibreSslCtxInternal, infoCallback) == 88);
static_assert(offsetof(LibreSslCtxInternal, nextProtoSelectCb) == 384);
static_assert(offsetof(LibreSslCtxInternal, nextProtoSelectCbArg) == 392);
static_assert(offsetof(LibreSslCtxInternal, alpnSelectCb) == 400);
static_assert(offsetof(LibreSslCtxInternal, alpnSelectCbArg) == 408);

namespace {

LibreSslCtxInternal& Callbacks(LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ctx->internal;
}

}

extern "C" {

void APS5_VABI SSL_CTX_set_info_callback_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).infoCallback = callback;
}

void APS5_VABI SSL_CTX_sess_set_new_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).newSessionCb = callback;
}

void APS5_VABI SSL_CTX_set_cert_verify_callback_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    LibreSslCtxInternal& callbacks = Callbacks(ctx, __func__);
    callbacks.appVerifyCallback = callback;
    callbacks.appVerifyArg = arg;
}

void APS5_VABI SSL_CTX_set_client_cert_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).clientCertCb = callback;
}

void APS5_VABI SSL_CTX_set_next_proto_select_cb_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    LibreSslCtxInternal& callbacks = Callbacks(ctx, __func__);
    callbacks.nextProtoSelectCb = callback;
    callbacks.nextProtoSelectCbArg = arg;
}

void APS5_VABI SSL_CTX_set_alpn_select_cb_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    LibreSslCtxInternal& callbacks = Callbacks(ctx, __func__);
    callbacks.alpnSelectCb = callback;
    callbacks.alpnSelectCbArg = arg;
}

}
