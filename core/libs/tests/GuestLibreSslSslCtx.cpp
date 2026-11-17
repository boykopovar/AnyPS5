#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

struct TestCtxInternal {
    unsigned char beforeExData[248];
    LibreSslStack* exData;
    unsigned char beforeQuietShutdown[36];
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

static_assert(sizeof(TestCtxInternal) == 464 && offsetof(TestCtxInternal, exData) == 248 && offsetof(TestCtxInternal, quietShutdown) == 292);
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

}

static void TestLoadVerifyLocations() {
    TestCtx ctx {};
    ctx.certStore = reinterpret_cast<void*>(1);
    std::array<unsigned char, sizeof(ctx)> before {};
    std::memcpy(before.data(), &ctx, sizeof(ctx));
    LibreSslPutError_nid_no_patch(32, 0xfff, 67);
    LibreSslPutError_nid_no_patch(32, 0xfff, 68);
    Require(SSL_CTX_load_verify_locations_nid_postfix(Context(&ctx), nullptr, nullptr) == 0);
    Require(std::memcmp(before.data(), &ctx, sizeof(ctx)) == 0);
    Require(ERR_get_error_nid_postfix() == 0x20fff043);
    Require(ERR_get_error_nid_postfix() == 0x20fff044);
    Require(ERR_get_error_nid_postfix() == 0);
    RequireThrows([] { SSL_CTX_load_verify_locations_nid_postfix(nullptr, nullptr, nullptr); });
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

static void TestSetExData() {
    TestCtxInternal internal {};
    TestCtx ctx {};
    int first = 1;
    int second = 2;
    RequireThrows([] { SSL_CTX_set_ex_data_nid_postfix(nullptr, 0, nullptr); });
    RequireThrows([&] { SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), 0, nullptr); });
    ctx.internal = &internal;
    Require(SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), 6, &first) == 1);
    Require(internal.exData && internal.exData->num == 7 && internal.exData->numAlloc == 8);
    Require(internal.exData->data[0] == nullptr && internal.exData->data[5] == nullptr);
    Require(internal.exData->data[6] == reinterpret_cast<char*>(&first));
    Require(SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), 6, &second) == 1);
    Require(internal.exData->data[6] == reinterpret_cast<char*>(&second));
    Require(SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), -1, nullptr) == 1);
    Require(internal.exData->num == 7 && internal.exData->data[6] == reinterpret_cast<char*>(&second));
    Require(internal.quietShutdown == 0);
    for (unsigned char byte : internal.beforeExData) Require(byte == 0);
    for (unsigned char byte : internal.beforeQuietShutdown) Require(byte == 0);
    for (unsigned char byte : internal.rest) Require(byte == 0);
    free_nid_postfix(internal.exData->data);
    free_nid_postfix(internal.exData);

    char* entries[4] {};
    LibreSslStack stack {2, entries, 1, 4, nullptr};
    entries[0] = reinterpret_cast<char*>(&first);
    internal.exData = &stack;
    Require(SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), 1, &second) == 1);
    Require(stack.num == 2 && entries[0] == reinterpret_cast<char*>(&first) && entries[1] == reinterpret_cast<char*>(&second));
    Require(stack.sorted == 1);
    Require(SSL_CTX_set_ex_data_nid_postfix(Context(&ctx), 2, nullptr) == 1);
    Require(stack.num == 3 && stack.sorted == 0 && entries[2] == nullptr);
    Require(internal.quietShutdown == 0);
}

static void TestGetExData() {
    int first = 1;
    int second = 2;
    char* slots[] = {reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second)};
    LibreSslStack stack {2, slots, 0, 2, nullptr};
    TestCtxInternal internal {};
    TestCtx ctx {};
    RequireThrows([] { SSL_CTX_get_ex_data_nid_postfix(nullptr, 0); });
    RequireThrows([&] { SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 0); });
    ctx.internal = &internal;
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 0) == nullptr);
    internal.exData = &stack;
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 0) == &first);
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 1) == &second);
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 2) == nullptr);
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), INT_MIN) == nullptr);
    internal.quietShutdown = 7;
    Require(SSL_CTX_get_ex_data_nid_postfix(Context(&ctx), 0) == &first);
}

int main() {
    void* heap[10] {};
    ApplicationHeapRegister_nid_no_patch(heap);
    TestUseCertificateNull();
    TestUseCertificateNonNull();
    TestGetVerifyMode();
    TestGetCertStore();
    TestSetCertStore();
    TestLoadVerifyLocations();
    TestSetQuietShutdown();
    TestSetPasswordCallback();
    TestSetPasswordUserdata();
    TestUsePrivateKey();
    TestSetExData();
    TestGetExData();
    return 0;
}
