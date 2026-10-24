#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestCertKey {
    void* x509;
    void* privatekey;
    const void* digest;
};

struct TestCert {
    TestCertKey* key;
    TestCertKey pkeys[8];
    unsigned char rest[56];
};

struct TestSession {
    unsigned char beforeCipher[168];
    const void* cipher;
    unsigned char rest[56];
};

struct TestSsl {
    unsigned char beforeCert[80];
    TestCert* cert;
    unsigned char beforeSession[40];
    TestSession* session;
    unsigned char rest[96];
};

static_assert(sizeof(TestCertKey) == 24 && sizeof(TestCert) == 256);
static_assert(sizeof(TestSession) == 232 && offsetof(TestSession, cipher) == 168);
static_assert(sizeof(TestSsl) == 232 && offsetof(TestSsl, cert) == 80 && offsetof(TestSsl, session) == 128);

extern "C" {
TestSession* APS5_VABI SSL_get_session_nid_postfix(const TestSsl*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL session check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGetSession() {
    TestSession session {};
    TestCert cert {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_session_nid_postfix(nullptr); });
    Require(SSL_get_session_nid_postfix(&ssl) == nullptr);
    ssl.cert = &cert;
    ssl.session = &session;
    Require(SSL_get_session_nid_postfix(&ssl) == &session);
}

int main() {
    TestGetSession();
    return 0;
}
