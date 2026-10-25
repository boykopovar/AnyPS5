#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestSslInternal {
    unsigned char beforeRwstate[156];
    int rwstate;
    unsigned char beforeShutdown[8];
    int shutdown;
    unsigned char rest[260];
};

struct TestSsl {
    int version;
    const void* method;
    void* rbio;
    void* wbio;
    void* bbio;
    unsigned char beforeSession[88];
    void* session;
    unsigned char beforeCtx[16];
    void* ctx;
    std::int64_t verifyResult;
    unsigned char beforeInternal[56];
    TestSslInternal* internal;
};

static_assert(sizeof(TestSslInternal) == 432 && offsetof(TestSslInternal, rwstate) == 156 && offsetof(TestSslInternal, shutdown) == 168);
static_assert(sizeof(TestSsl) == 232 && offsetof(TestSsl, session) == 128 && offsetof(TestSsl, ctx) == 152);
static_assert(offsetof(TestSsl, verifyResult) == 160 && offsetof(TestSsl, internal) == 224);

static const LibreSsl* Ssl(const TestSsl* ssl) {
    return reinterpret_cast<const LibreSsl*>(ssl);
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

int main() {
    TestGetSslCtx();
    TestGetRbio();
    TestGetWbio();
    TestVersion();
    TestVerifyResult();
    TestGetShutdown();
    TestWant();
    return 0;
}
