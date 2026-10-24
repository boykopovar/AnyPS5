#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslCertKey {
    void* x509;
    void* privatekey;
    const void* digest;
};

struct LibreSslCert {
    LibreSslCertKey* key;
    unsigned char rest[248];
};

struct LibreSslSession {
    unsigned char beforeCipher[168];
    const void* cipher;
    unsigned char rest[56];
};

struct LibreSslSessionConnection {
    unsigned char beforeCert[80];
    LibreSslCert* cert;
    unsigned char beforeSession[40];
    LibreSslSession* session;
    unsigned char rest[96];
};

static_assert(sizeof(LibreSslCertKey) == 24);
static_assert(offsetof(LibreSslCertKey, privatekey) == 8);
static_assert(sizeof(LibreSslCert) == 256);
static_assert(sizeof(LibreSslSession) == 232);
static_assert(offsetof(LibreSslSession, cipher) == 168);
static_assert(sizeof(LibreSslSessionConnection) == 232);
static_assert(offsetof(LibreSslSessionConnection, cert) == 80);
static_assert(offsetof(LibreSslSessionConnection, session) == 128);

namespace {

const LibreSslSessionConnection& Connection(const LibreSslSessionConnection* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslCertKey* CurrentKey(const LibreSslSessionConnection* ssl, const char* function) {
    const LibreSslCert* cert = Connection(ssl, function).cert;
    if (!cert) return nullptr;
    if (!cert->key) throw std::invalid_argument(std::string(function) + ": null current key");
    return cert->key;
}

}

extern "C" {

LibreSslSession* APS5_VABI SSL_get_session_nid_postfix(const LibreSslSessionConnection* ssl) {
    return Connection(ssl, __func__).session;
}

}
