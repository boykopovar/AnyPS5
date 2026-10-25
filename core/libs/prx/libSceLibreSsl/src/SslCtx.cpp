#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxInternal) == 464);
static_assert(offsetof(LibreSslCtxInternal, quietShutdown) == 292);
static_assert(sizeof(LibreSslCtx) == 120);
static_assert(offsetof(LibreSslCtx, certStore) == 16);
static_assert(offsetof(LibreSslCtx, verifyMode) == 48);
static_assert(offsetof(LibreSslCtx, defaultPasswdCallback) == 96);
static_assert(offsetof(LibreSslCtx, defaultPasswdCallbackUserdata) == 104);
static_assert(offsetof(LibreSslCtx, internal) == 112);

namespace {

LibreSslCtx& Context(LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    return *ctx;
}

}

extern "C" {

int APS5_VABI SSL_CTX_get_verify_mode_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).verifyMode;
}

void* APS5_VABI SSL_CTX_get_cert_store_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).certStore;
}

void APS5_VABI SSL_CTX_set_quiet_shutdown_nid_postfix(LibreSslCtx* ctx, int mode) {
    LibreSslCtx& context = Context(ctx, __func__);
    if (!context.internal) throw std::invalid_argument(std::string(__func__) + ": null internal state");
    context.internal->quietShutdown = mode;
}

void APS5_VABI SSL_CTX_set_default_passwd_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Context(ctx, __func__).defaultPasswdCallback = callback;
}

void APS5_VABI SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(LibreSslCtx* ctx, void* userdata) {
    Context(ctx, __func__).defaultPasswdCallbackUserdata = userdata;
}

}
