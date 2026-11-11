#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static void TestUsePrivateKey() {
    constexpr std::uint64_t nullParameter = 0x14fff043;
    TestCtx ctx {};
    unsigned char original[sizeof(ctx)];
    std::memcpy(original, &ctx, sizeof(ctx));
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(8, 0xfff, 101);
    Require(SSL_CTX_use_PrivateKey_nid_postfix(nullptr, nullptr) == 0);
    Require(SSL_CTX_use_PrivateKey_nid_postfix(Context(&ctx), nullptr) == 0);
    Require(std::memcmp(&ctx, original, sizeof(ctx)) == 0);
    Require(ERR_get_error_nid_postfix() == 0x08fff065);
    Require(ERR_get_error_nid_postfix() == nullParameter);
    Require(ERR_get_error_nid_postfix() == nullParameter);

    auto* const inaccessible = reinterpret_cast<LibreSslCtx*>(1);
    Require(SSL_CTX_use_PrivateKey_nid_postfix(inaccessible, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == nullParameter);

    RequireThrows([] { SSL_CTX_use_PrivateKey_nid_postfix(nullptr, reinterpret_cast<void*>(1)); });
    RequireThrows([&] { SSL_CTX_use_PrivateKey_nid_postfix(inaccessible, reinterpret_cast<void*>(1)); });
    Require(std::memcmp(&ctx, original, sizeof(ctx)) == 0);
    Require(ERR_get_error_nid_postfix() == 0);
}

int main() {
    TestGetVerifyMode();
    TestGetCertStore();
    TestSetQuietShutdown();
    TestSetPasswordCallback();
    TestSetPasswordUserdata();
    TestUsePrivateKey();
    return 0;
}
