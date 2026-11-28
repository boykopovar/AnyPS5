#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

struct TestSslInternal {
    unsigned char beforeRwstate[156];
    int rwstate;
    unsigned char beforeShutdown[8];
    int shutdown;
    unsigned char beforeExData[100];
    LibreSslExData exData;
    unsigned char afterExData[152];
};

struct TestSsl {
    int version;
    const void* method;
    void* rbio;
    void* wbio;
    void* bbio;
    int server;
    void* s3;
    void* d1;
    void* param;
    void* cipherList;
    void* cert;
    unsigned int sidCtxLength;
    unsigned char sidCtx[32];
    void* session;
    int verifyMode;
    int error;
    int errorCode;
    int beforeCtx;
    void* ctx;
    std::int64_t verifyResult;
    unsigned char beforeInternal[56];
    TestSslInternal* internal;
};

static_assert(sizeof(TestSslInternal) == 432 && offsetof(TestSslInternal, rwstate) == 156 && offsetof(TestSslInternal, shutdown) == 168);
static_assert(offsetof(TestSslInternal, exData) == 272);
static_assert(sizeof(TestSsl) == 232 && offsetof(TestSsl, cert) == 80 && offsetof(TestSsl, session) == 128 && offsetof(TestSsl, error) == 140 && offsetof(TestSsl, errorCode) == 144 && offsetof(TestSsl, ctx) == 152);
static_assert(offsetof(TestSsl, verifyResult) == 160 && offsetof(TestSsl, internal) == 224);

static LibreSslSsl* Ssl(TestSsl* ssl) {
    return reinterpret_cast<LibreSslSsl*>(ssl);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestCheckPrivateKey() {
    constexpr std::uint64_t firstCode = (8ULL << 24) | (0xfffULL << 12) | 101;
    constexpr std::uint64_t nullSslCode = (20ULL << 24) | (0xfffULL << 12) | 67;
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(8, 0xfff, 101);
    Require(SSL_check_private_key_nid_postfix(nullptr) == 0);
    Require(ERR_peek_error_nid_postfix() == firstCode);
    Require(ERR_get_error_nid_postfix() == firstCode);
    Require(ERR_get_error_nid_postfix() == nullSslCode);
    Require(ERR_get_error_nid_postfix() == 0);

    RequireThrows([] { SSL_check_private_key_nid_postfix(reinterpret_cast<const LibreSslSsl*>(std::uintptr_t{1})); });
    Require(ERR_peek_error_nid_postfix() == 0);

    TestSsl ssl;
    std::memset(&ssl, 0xa5, sizeof(ssl));
    ssl.cert = reinterpret_cast<void*>(std::uintptr_t{1});
    ssl.error = 0x12345678;
    ssl.errorCode = 0x23456789;
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    ERR_clear_error_nid_postfix();
    RequireThrows([&] { SSL_check_private_key_nid_postfix(Ssl(&ssl)); });
    Require(ERR_peek_error_nid_postfix() == 0);
    Require(std::memcmp(before, &ssl, sizeof(ssl)) == 0);
}

static void TestGetSslCtx() {
    int context = 0;
    int session = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_SSL_CTX_nid_postfix(nullptr); });
    Require(SSL_get_SSL_CTX_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.ctx = &context;
    ssl.session = &session;
    Require(SSL_get_SSL_CTX_nid_postfix(Ssl(&ssl)) == &context);
}

static void TestGet1Session() {
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    LibreSslPutError_nid_no_patch(4, 5, 6);
    constexpr std::uint64_t firstError = (std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3;
    constexpr std::uint64_t secondError = (std::uint64_t{4} << 24) | (std::uint64_t{5} << 12) | 6;
    TestSsl ssl {};
    std::array<unsigned char, sizeof(TestSsl)> before {};
    std::memcpy(before.data(), &ssl, sizeof(ssl));
    RequireThrows([] { SSL_get1_session_nid_postfix(nullptr); });
    Require(std::memcmp(&ssl, before.data(), sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(LibreSslPeekLastError_nid_no_patch() == secondError);
    Require(SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before.data(), sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(LibreSslPeekLastError_nid_no_patch() == secondError);
    Require(ERR_get_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == secondError);
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetRbio() {
    int read = 0;
    int write = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_rbio_nid_postfix(nullptr); });
    Require(SSL_get_rbio_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    Require(SSL_get_rbio_nid_postfix(Ssl(&ssl)) == &read);
}

static void TestGetWbio() {
    int read = 0;
    int write = 0;
    int buffer = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_wbio_nid_postfix(nullptr); });
    Require(SSL_get_wbio_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    ssl.bbio = &buffer;
    Require(SSL_get_wbio_nid_postfix(Ssl(&ssl)) == &write);
}

static void TestVersion() {
    TestSsl ssl {};
    RequireThrows([] { SSL_version_nid_postfix(nullptr); });
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0);
    ssl.version = 0x0303;
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0x0303);
    ssl.version = 0xFEFF;
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0xFEFF);
}

static void TestVerifyResult() {
    TestSsl ssl {};
    RequireThrows([] { SSL_get_verify_result_nid_postfix(nullptr); });
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == 0);
    ssl.verifyResult = 20;
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == 20);
    ssl.verifyResult = INT64_C(0x100000002);
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == INT64_C(0x100000002));
    ssl.verifyResult = -1;
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == -1);
}

static void TestGetPeerCertificateNullPaths() {
    LibreSslPutError_nid_no_patch(1, 2, 3);
    LibreSslPutError_nid_no_patch(4, 5, 6);
    TestSsl ssl {};
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    Require(SSL_get_peer_certificate_nid_postfix(nullptr) == nullptr);
    Require(SSL_get_peer_certificate_nid_postfix(Ssl(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == ((std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3));
    Require(ERR_get_error_nid_postfix() == ((std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3));
    Require(ERR_get_error_nid_postfix() == ((std::uint64_t{4} << 24) | (std::uint64_t{5} << 12) | 6));
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetShutdown() {
    TestSslInternal internal {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_shutdown_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_get_shutdown_nid_postfix(Ssl(&ssl)); });
    ssl.internal = &internal;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 0);
    internal.rwstate = 3;
    internal.shutdown = 1;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 1);
    internal.shutdown = 3;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 3);
}

static void TestWant() {
    TestSslInternal internal {};
    TestSsl ssl {};
    RequireThrows([] { SSL_want_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_want_nid_postfix(Ssl(&ssl)); });
    ssl.internal = &internal;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 0);
    internal.rwstate = 1;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 1);
    internal.shutdown = 2;
    internal.rwstate = 3;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 3);
}

static void FreeExData(TestSslInternal& internal) {
    auto* const stack = static_cast<LibreSslStack*>(internal.exData.stack);
    if (!stack) return;
    CRYPTO_free_nid_postfix(stack->data);
    CRYPTO_free_nid_postfix(stack);
    internal.exData.stack = nullptr;
}

static void TestSetExData() {
    TestSslInternal internal {};
    std::memset(internal.beforeExData, 0xa5, sizeof(internal.beforeExData));
    std::memset(internal.afterExData, 0x5a, sizeof(internal.afterExData));
    TestSsl ssl {};
    int first = 1;
    int replacement = 2;
    RequireThrows([] { SSL_set_ex_data_nid_postfix(nullptr, 0, nullptr); });
    RequireThrows([&] { SSL_set_ex_data_nid_postfix(Ssl(&ssl), 0, nullptr); });
    ssl.internal = &internal;
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), -1, &first) == 1);
    auto* stack = static_cast<LibreSslStack*>(internal.exData.stack);
    Require(stack && stack->num == 0 && stack->numAlloc == 4);
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &first) == 1);
    Require(stack->num == 3 && stack->data[0] == nullptr && stack->data[1] == nullptr && stack->data[2] == reinterpret_cast<char*>(&first));
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &replacement) == 1);
    Require(stack->data[2] == reinterpret_cast<char*>(&replacement));
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 5, nullptr) == 1);
    Require(stack->num == 6 && stack->data[3] == nullptr && stack->data[4] == nullptr && stack->data[5] == nullptr);
    Require(internal.rwstate == 0 && internal.shutdown == 0);
    for (unsigned char byte : internal.beforeExData) Require(byte == 0xa5);
    for (unsigned char byte : internal.afterExData) Require(byte == 0x5a);
    FreeExData(internal);
}

static void TestSetExDataPrefilledStack() {
    TestSslInternal internal {};
    TestSsl ssl {};
    ssl.internal = &internal;
    auto* stack = static_cast<LibreSslStack*>(CRYPTO_malloc_nid_postfix(sizeof(LibreSslStack), nullptr, 0));
    Require(stack != nullptr);
    stack->data = static_cast<char**>(CRYPTO_malloc_nid_postfix(4 * sizeof(char*), nullptr, 0));
    Require(stack->data != nullptr);
    int original = 1;
    int updated = 2;
    stack->num = 1;
    stack->data[0] = reinterpret_cast<char*>(&original);
    stack->data[1] = nullptr;
    stack->data[2] = nullptr;
    stack->data[3] = nullptr;
    stack->sorted = 1;
    stack->numAlloc = 4;
    stack->comp = nullptr;
    internal.exData.stack = stack;
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 0, &updated) == 1);
    Require(stack->num == 1 && stack->data[0] == reinterpret_cast<char*>(&updated) && stack->sorted == 1);
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &original) == 1);
    Require(stack->num == 3 && stack->data[1] == nullptr && stack->data[2] == reinterpret_cast<char*>(&original) && stack->sorted == 0);
    FreeExData(internal);
}

static void TestGetPeerCertChain() {
    TestSsl ssl {};
    LibreSslPutError_nid_no_patch(3, 100, 65);
    LibreSslPutError_nid_no_patch(4, 101, 66);
    constexpr std::uint64_t firstError = (std::uint64_t{3} << 24) | (std::uint64_t{100} << 12) | 65;
    constexpr std::uint64_t secondError = (std::uint64_t{4} << 24) | (std::uint64_t{101} << 12) | 66;
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(SSL_get_peer_cert_chain_nid_postfix(nullptr) == nullptr);
    Require(ERR_peek_error_nid_postfix() == firstError);
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    int session = 0;
    ssl.session = &session;
    std::memcpy(before, &ssl, sizeof(ssl));
    RequireThrows([&] { SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)); });
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == secondError);
    Require(ERR_get_error_nid_postfix() == 0);
}

int main() {
    void* heap[10] {};
    ApplicationHeapRegister_nid_no_patch(heap);
    TestGet1Session();
    TestCheckPrivateKey();
    TestGetSslCtx();
    TestGetRbio();
    TestGetWbio();
    TestVersion();
    TestVerifyResult();
    TestGetPeerCertificateNullPaths();
    TestGetShutdown();
    TestWant();
    TestSetExData();
    TestSetExDataPrefilledStack();
    TestGetPeerCertChain();
    return 0;
}
