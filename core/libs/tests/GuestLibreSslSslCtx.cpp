#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestCtxInternal {
    unsigned char beforeQuietShutdown[292];
    int quietShutdown;
    unsigned char rest[168];
};

struct TestCtx {
    const void* method;
    void* cipherList;
    void* certStore;
    std::int64_t sessionTimeout;
    unsigned char beforeVerifyMode[16];
    int verifyMode;
    unsigned char beforeCallback[44];
    void* defaultPasswdCallback;
    void* defaultPasswdCallbackUserdata;
    TestCtxInternal* internal;
};

static_assert(sizeof(TestCtxInternal) == 464 && offsetof(TestCtxInternal, quietShutdown) == 292);
static_assert(sizeof(TestCtx) == 120 && offsetof(TestCtx, verifyMode) == 48 && offsetof(TestCtx, defaultPasswdCallback) == 96);

extern "C" {
int APS5_VABI SSL_CTX_get_verify_mode_nid_postfix(TestCtx*);
void* APS5_VABI SSL_CTX_get_cert_store_nid_postfix(TestCtx*);
void APS5_VABI SSL_CTX_set_quiet_shutdown_nid_postfix(TestCtx*, int);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL_CTX check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGetVerifyMode() {
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_get_verify_mode_nid_postfix(nullptr); });
    Require(SSL_CTX_get_verify_mode_nid_postfix(&ctx) == 0);
    ctx.verifyMode = 0x03;
    Require(SSL_CTX_get_verify_mode_nid_postfix(&ctx) == 0x03);
    ctx.verifyMode = -1;
    Require(SSL_CTX_get_verify_mode_nid_postfix(&ctx) == -1);
}

static void TestGetCertStore() {
    int store = 0;
    int ciphers = 0;
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_get_cert_store_nid_postfix(nullptr); });
    Require(SSL_CTX_get_cert_store_nid_postfix(&ctx) == nullptr);
    ctx.cipherList = &ciphers;
    ctx.certStore = &store;
    ctx.sessionTimeout = -1;
    Require(SSL_CTX_get_cert_store_nid_postfix(&ctx) == &store);
}

static void TestSetQuietShutdown() {
    TestCtxInternal internal {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_set_quiet_shutdown_nid_postfix(nullptr, 1); });
    RequireThrows([&] { SSL_CTX_set_quiet_shutdown_nid_postfix(&ctx, 1); });
    ctx.internal = &internal;
    SSL_CTX_set_quiet_shutdown_nid_postfix(&ctx, 1);
    Require(internal.quietShutdown == 1);
    SSL_CTX_set_quiet_shutdown_nid_postfix(&ctx, INT_MIN);
    Require(internal.quietShutdown == INT_MIN);
    SSL_CTX_set_quiet_shutdown_nid_postfix(&ctx, 0);
    Require(internal.quietShutdown == 0);
    for (unsigned char byte : internal.beforeQuietShutdown) Require(byte == 0);
    for (unsigned char byte : internal.rest) Require(byte == 0);
}

int main() {
    TestGetVerifyMode();
    TestGetCertStore();
    TestSetQuietShutdown();
    return 0;
}
