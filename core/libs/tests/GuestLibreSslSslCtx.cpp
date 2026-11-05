#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

static LibreSslCtx* Context(TestCtx* ctx) {
    return reinterpret_cast<LibreSslCtx*>(ctx);
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

static void TestUseCertificateNull() {
    constexpr std::uint64_t nullCertificateError = (20ULL << 24) | (0xfffULL << 12) | 67;
    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_certificate_nid_postfix(nullptr, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == nullCertificateError);
    Require(ERR_get_error_nid_postfix() == 0);

    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_certificate_nid_postfix(reinterpret_cast<LibreSslCtx*>(1), nullptr) == 0);
    Require(ERR_peek_error_nid_postfix() == nullCertificateError);
    Require(ERR_get_error_nid_postfix() == nullCertificateError);
    Require(ERR_get_error_nid_postfix() == 0);

    TestCtx ctx;
    TestCtxInternal internal;
    auto* ctxBytes = reinterpret_cast<unsigned char*>(&ctx);
    auto* internalBytes = reinterpret_cast<unsigned char*>(&internal);
    for (std::size_t index = 0; index < sizeof(ctx); ++index) ctxBytes[index] = static_cast<unsigned char>(index * 13 + 7);
    for (std::size_t index = 0; index < sizeof(internal); ++index) internalBytes[index] = static_cast<unsigned char>(index * 17 + 11);
    ctx.internal = &internal;
    unsigned char ctxBefore[sizeof(ctx)];
    unsigned char internalBefore[sizeof(internal)];
    std::memcpy(ctxBefore, &ctx, sizeof(ctx));
    std::memcpy(internalBefore, &internal, sizeof(internal));
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    Require(SSL_CTX_use_certificate_nid_postfix(Context(&ctx), nullptr) == 0);
    Require(std::memcmp(ctxBefore, &ctx, sizeof(ctx)) == 0);
    Require(std::memcmp(internalBefore, &internal, sizeof(internal)) == 0);
    Require(ERR_get_error_nid_postfix() == ((1ULL << 24) | (2ULL << 12) | 3));
    Require(ERR_get_error_nid_postfix() == nullCertificateError);
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestUseCertificateNonNull() {
    int certificate = 0;
    ERR_clear_error_nid_postfix();
    RequireThrows([&] { SSL_CTX_use_certificate_nid_postfix(nullptr, &certificate); });
    Require(ERR_get_error_nid_postfix() == 0);
    TestCtx ctx;
    RequireThrows([&] { SSL_CTX_use_certificate_nid_postfix(Context(&ctx), &certificate); });
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetVerifyMode() {
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_get_verify_mode_nid_postfix(nullptr); });
    Require(SSL_CTX_get_verify_mode_nid_postfix(Context(&ctx)) == 0);
    ctx.verifyMode = 0x03;
    Require(SSL_CTX_get_verify_mode_nid_postfix(Context(&ctx)) == 0x03);
    ctx.verifyMode = -1;
    Require(SSL_CTX_get_verify_mode_nid_postfix(Context(&ctx)) == -1);
}

static void TestGetCertStore() {
    int store = 0;
    int ciphers = 0;
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_get_cert_store_nid_postfix(nullptr); });
    Require(SSL_CTX_get_cert_store_nid_postfix(Context(&ctx)) == nullptr);
    ctx.cipherList = &ciphers;
    ctx.certStore = &store;
    ctx.sessionTimeout = -1;
    Require(SSL_CTX_get_cert_store_nid_postfix(Context(&ctx)) == &store);
}

static void TestSetQuietShutdown() {
    TestCtxInternal internal {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_set_quiet_shutdown_nid_postfix(nullptr, 1); });
    RequireThrows([&] { SSL_CTX_set_quiet_shutdown_nid_postfix(Context(&ctx), 1); });
    ctx.internal = &internal;
    SSL_CTX_set_quiet_shutdown_nid_postfix(Context(&ctx), 1);
    Require(internal.quietShutdown == 1);
    SSL_CTX_set_quiet_shutdown_nid_postfix(Context(&ctx), INT_MIN);
    Require(internal.quietShutdown == INT_MIN);
    SSL_CTX_set_quiet_shutdown_nid_postfix(Context(&ctx), 0);
    Require(internal.quietShutdown == 0);
    for (unsigned char byte : internal.beforeQuietShutdown) Require(byte == 0);
    for (unsigned char byte : internal.rest) Require(byte == 0);
}

static void TestSetPasswordCallback() {
    int callback = 0;
    TestCtx ctx {};
    RequireThrows([&] { SSL_CTX_set_default_passwd_cb_nid_postfix(nullptr, &callback); });
    SSL_CTX_set_default_passwd_cb_nid_postfix(Context(&ctx), &callback);
    Require(ctx.defaultPasswdCallback == &callback && ctx.defaultPasswdCallbackUserdata == nullptr);
    SSL_CTX_set_default_passwd_cb_nid_postfix(Context(&ctx), nullptr);
    Require(ctx.defaultPasswdCallback == nullptr);
}

static void TestSetPasswordUserdata() {
    int userdata = 0;
    TestCtx ctx {};
    RequireThrows([&] { SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(nullptr, &userdata); });
    SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(Context(&ctx), &userdata);
    Require(ctx.defaultPasswdCallbackUserdata == &userdata && ctx.defaultPasswdCallback == nullptr && ctx.internal == nullptr);
    SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(Context(&ctx), nullptr);
    Require(ctx.defaultPasswdCallbackUserdata == nullptr);
}

int main() {
    TestUseCertificateNull();
    TestUseCertificateNonNull();
    TestGetVerifyMode();
    TestGetCertStore();
    TestSetQuietShutdown();
    TestSetPasswordCallback();
    TestSetPasswordUserdata();
    return 0;
}
