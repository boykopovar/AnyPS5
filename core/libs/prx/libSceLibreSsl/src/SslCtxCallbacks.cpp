#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxCallbacks) == 464);
static_assert(offsetof(LibreSslCtxCallbacks, newSessionCb) == 24);
static_assert(offsetof(LibreSslCtxCallbacks, appVerifyCallback) == 48);
static_assert(offsetof(LibreSslCtxCallbacks, appVerifyArg) == 56);
static_assert(offsetof(LibreSslCtxCallbacks, clientCertCb) == 64);
static_assert(offsetof(LibreSslCtxCallbacks, infoCallback) == 88);
static_assert(offsetof(LibreSslCtxCallbacks, nextProtoSelectCb) == 384);
static_assert(offsetof(LibreSslCtxCallbacks, nextProtoSelectCbArg) == 392);
static_assert(offsetof(LibreSslCtxCallbacks, alpnSelectCb) == 400);
static_assert(offsetof(LibreSslCtxCallbacks, alpnSelectCbArg) == 408);
static_assert(sizeof(LibreSslCallbackCtx) == 120);

namespace {

LibreSslCtxCallbacks& Callbacks(LibreSslCallbackCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ctx->internal;
}

}

extern "C" {

void APS5_VABI SSL_CTX_set_info_callback_nid_postfix(LibreSslCallbackCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).infoCallback = callback;
}

void APS5_VABI SSL_CTX_sess_set_new_cb_nid_postfix(LibreSslCallbackCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).newSessionCb = callback;
}

void APS5_VABI SSL_CTX_set_cert_verify_callback_nid_postfix(LibreSslCallbackCtx* ctx, void* callback, void* arg) {
    LibreSslCtxCallbacks& callbacks = Callbacks(ctx, __func__);
    callbacks.appVerifyCallback = callback;
    callbacks.appVerifyArg = arg;
}

void APS5_VABI SSL_CTX_set_client_cert_cb_nid_postfix(LibreSslCallbackCtx* ctx, void* callback) {
    Callbacks(ctx, __func__).clientCertCb = callback;
}

void APS5_VABI SSL_CTX_set_next_proto_select_cb_nid_postfix(LibreSslCallbackCtx* ctx, void* callback, void* arg) {
    LibreSslCtxCallbacks& callbacks = Callbacks(ctx, __func__);
    callbacks.nextProtoSelectCb = callback;
    callbacks.nextProtoSelectCbArg = arg;
}

void APS5_VABI SSL_CTX_set_alpn_select_cb_nid_postfix(LibreSslCallbackCtx* ctx, void* callback, void* arg) {
    LibreSslCtxCallbacks& callbacks = Callbacks(ctx, __func__);
    callbacks.alpnSelectCb = callback;
    callbacks.alpnSelectCbArg = arg;
}

}
