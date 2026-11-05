#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
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

static void TestSetCertStore() {
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_set_cert_store_nid_postfix(nullptr, nullptr); });
    auto* oldStore = X509_STORE_new_nid_postfix();
    auto* store = X509_STORE_new_nid_postfix();
    Require(oldStore != nullptr && store != nullptr);
    oldStore->references = 2;
    store->references = 2;
    ctx.certStore = oldStore;
    ctx.cipherList = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    SSL_CTX_set_cert_store_nid_postfix(Context(&ctx), store);
    Require(ctx.certStore == store && ctx.cipherList == reinterpret_cast<void*>(std::uintptr_t{0x1234}));
    Require(oldStore->references == 1 && store->references == 2);
    X509_STORE_free_nid_postfix(oldStore);
    SSL_CTX_set_cert_store_nid_postfix(Context(&ctx), nullptr);
    Require(ctx.certStore == nullptr && store->references == 1);
    X509_STORE_free_nid_postfix(store);

    auto* clearedStore = X509_STORE_new_nid_postfix();
    Require(clearedStore != nullptr);
    clearedStore->references = 2;
    ctx.certStore = clearedStore;
    SSL_CTX_set_cert_store_nid_postfix(Context(&ctx), nullptr);
    Require(ctx.certStore == nullptr && clearedStore->references == 1);
    X509_STORE_free_nid_postfix(clearedStore);

    auto* preservedStore = X509_STORE_new_nid_postfix();
    Require(preservedStore != nullptr);
    char* elements[] = {reinterpret_cast<char*>(std::uintptr_t{1})};
    LibreSslStack objectStack {1, elements, 0, 1, nullptr};
    LibreSslX509Store unsupportedStore {};
    unsupportedStore.objects = &objectStack;
    unsupportedStore.references = 1;
    ctx.certStore = &unsupportedStore;
    RequireThrows([&] { SSL_CTX_set_cert_store_nid_postfix(Context(&ctx), preservedStore); });
    Require(ctx.certStore == &unsupportedStore && unsupportedStore.references == 0);
    Require(preservedStore->references == 1);
    ctx.certStore = preservedStore;
    SSL_CTX_set_cert_store_nid_postfix(Context(&ctx), nullptr);
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
    void* heap[10] {};
    ApplicationHeapRegister_nid_no_patch(heap);
    TestGetVerifyMode();
    TestGetCertStore();
    TestSetCertStore();
    TestSetQuietShutdown();
    TestSetPasswordCallback();
    TestSetPasswordUserdata();
    return 0;
}
