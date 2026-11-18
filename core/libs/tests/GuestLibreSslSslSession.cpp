#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
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
    unsigned char beforeReferences[160];
    int references;
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
static_assert(sizeof(TestSession) == 232 && offsetof(TestSession, references) == 160 && offsetof(TestSession, cipher) == 168);
static_assert(sizeof(TestSsl) == 232 && offsetof(TestSsl, cert) == 80 && offsetof(TestSsl, session) == 128);

static const LibreSslSsl* Connection(const TestSsl* ssl) {
    return reinterpret_cast<const LibreSslSsl*>(ssl);
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
    Require(SSL_get_session_nid_postfix(Connection(&ssl)) == nullptr);
    ssl.cert = &cert;
    ssl.session = &session;
    Require(reinterpret_cast<TestSession*>(SSL_get_session_nid_postfix(Connection(&ssl))) == &session);
}

static void TestGet1Session() {
    TestSession session {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get1_session_nid_postfix(nullptr); });
    Require(SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)) == nullptr);
    session.references = 1;
    ssl.session = &session;
    Require(SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)) == &session);
    Require(session.references == 2);
    Require(SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)) == &session);
    Require(session.references == 3);
    session.references = INT_MAX;
    RequireThrows([&] { SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)); });
    Require(session.references == INT_MAX);
}

static void TestSessionFree() {
    TestSession session {};
    SSL_SESSION_free_nid_postfix(nullptr);
    session.references = 3;
    SSL_SESSION_free_nid_postfix(&session);
    Require(session.references == 2);
    SSL_SESSION_free_nid_postfix(&session);
    Require(session.references == 1);
    RequireThrows([&] { SSL_SESSION_free_nid_postfix(&session); });
    Require(session.references == 1);
    session.references = 0;
    RequireThrows([&] { SSL_SESSION_free_nid_postfix(&session); });
    Require(session.references == 0);
    session.references = -1;
    RequireThrows([&] { SSL_SESSION_free_nid_postfix(&session); });
    Require(session.references == -1);
}

static void TestCurrentCipher() {
    int cipher = 0;
    TestSession session {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_current_cipher_nid_postfix(nullptr); });
    Require(SSL_get_current_cipher_nid_postfix(Connection(&ssl)) == nullptr);
    ssl.session = &session;
    Require(SSL_get_current_cipher_nid_postfix(Connection(&ssl)) == nullptr);
    session.cipher = &cipher;
    Require(SSL_get_current_cipher_nid_postfix(Connection(&ssl)) == &cipher);
}

static void TestGetCertificate() {
    int x509 = 0;
    int other = 0;
    int key = 0;
    TestCert cert {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_certificate_nid_postfix(nullptr); });
    Require(SSL_get_certificate_nid_postfix(Connection(&ssl)) == nullptr);
    ssl.cert = &cert;
    RequireThrows([&] { SSL_get_certificate_nid_postfix(Connection(&ssl)); });
    cert.pkeys[0].x509 = &other;
    cert.pkeys[2].x509 = &x509;
    cert.pkeys[2].privatekey = &key;
    cert.key = &cert.pkeys[1];
    Require(SSL_get_certificate_nid_postfix(Connection(&ssl)) == nullptr);
    cert.key = &cert.pkeys[2];
    Require(SSL_get_certificate_nid_postfix(Connection(&ssl)) == &x509);
}

static void TestGetPrivateKey() {
    int x509 = 0;
    int key = 0;
    int other = 0;
    TestCert cert {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_privatekey_nid_postfix(nullptr); });
    Require(SSL_get_privatekey_nid_postfix(Connection(&ssl)) == nullptr);
    ssl.cert = &cert;
    RequireThrows([&] { SSL_get_privatekey_nid_postfix(Connection(&ssl)); });
    cert.pkeys[0].privatekey = &other;
    cert.pkeys[3].x509 = &x509;
    cert.pkeys[3].privatekey = &key;
    cert.key = &cert.pkeys[1];
    Require(SSL_get_privatekey_nid_postfix(Connection(&ssl)) == nullptr);
    cert.key = &cert.pkeys[3];
    Require(SSL_get_privatekey_nid_postfix(Connection(&ssl)) == &key);
}

int main() {
    TestGetSession();
    TestGet1Session();
    TestSessionFree();
    TestCurrentCipher();
    TestGetCertificate();
    TestGetPrivateKey();
    return 0;
}
