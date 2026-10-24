#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslCtxInternal {
    unsigned char beforeQuietShutdown[292];
    int quietShutdown;
    unsigned char rest[168];
};

struct LibreSslCtx {
    const void* method;
    void* cipherList;
    void* certStore;
    std::int64_t sessionTimeout;
    int references;
    void* extraCerts;
    int verifyMode;
    unsigned int sidCtxLength;
    unsigned char sidCtx[32];
    void* param;
    void* defaultPasswdCallback;
    void* defaultPasswdCallbackUserdata;
    LibreSslCtxInternal* internal;
};

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

}
