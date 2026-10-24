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

extern "C" {
void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const TestSsl*);
void* APS5_VABI SSL_get_rbio_nid_postfix(const TestSsl*);
void* APS5_VABI SSL_get_wbio_nid_postfix(const TestSsl*);
int APS5_VABI SSL_version_nid_postfix(const TestSsl*);
std::int64_t APS5_VABI SSL_get_verify_result_nid_postfix(const TestSsl*);
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
    Require(SSL_get_SSL_CTX_nid_postfix(&ssl) == nullptr);
    ssl.ctx = &context;
    ssl.session = &session;
    Require(SSL_get_SSL_CTX_nid_postfix(&ssl) == &context);
}

static void TestGetRbio() {
    int read = 0;
    int write = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_rbio_nid_postfix(nullptr); });
    Require(SSL_get_rbio_nid_postfix(&ssl) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    Require(SSL_get_rbio_nid_postfix(&ssl) == &read);
}

static void TestGetWbio() {
    int read = 0;
    int write = 0;
    int buffer = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_wbio_nid_postfix(nullptr); });
    Require(SSL_get_wbio_nid_postfix(&ssl) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    ssl.bbio = &buffer;
    Require(SSL_get_wbio_nid_postfix(&ssl) == &write);
}

static void TestVersion() {
    TestSsl ssl {};
    RequireThrows([] { SSL_version_nid_postfix(nullptr); });
    Require(SSL_version_nid_postfix(&ssl) == 0);
    ssl.version = 0x0303;
    Require(SSL_version_nid_postfix(&ssl) == 0x0303);
    ssl.version = 0xFEFF;
    Require(SSL_version_nid_postfix(&ssl) == 0xFEFF);
}

static void TestVerifyResult() {
    TestSsl ssl {};
    RequireThrows([] { SSL_get_verify_result_nid_postfix(nullptr); });
    Require(SSL_get_verify_result_nid_postfix(&ssl) == 0);
    ssl.verifyResult = 20;
    Require(SSL_get_verify_result_nid_postfix(&ssl) == 20);
    ssl.verifyResult = INT64_C(0x100000002);
    Require(SSL_get_verify_result_nid_postfix(&ssl) == INT64_C(0x100000002));
    ssl.verifyResult = -1;
    Require(SSL_get_verify_result_nid_postfix(&ssl) == -1);
}

int main() {
    TestGetSslCtx();
    TestGetRbio();
    TestGetWbio();
    TestVersion();
    TestVerifyResult();
    return 0;
}
