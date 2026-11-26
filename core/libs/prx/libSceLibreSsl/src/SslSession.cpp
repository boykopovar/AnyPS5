#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstring>

static_assert(sizeof(LibreSslCertKey) == 24);
static_assert(offsetof(LibreSslCertKey, privatekey) == 8);
static_assert(sizeof(LibreSslCert) == 256);
static_assert(sizeof(LibreSslSession) == 232);
static_assert(offsetof(LibreSslSession, cipher) == 168);

namespace {

const LibreSslSsl& Connection(const LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSession* Session(const LibreSslSsl* ssl, const char* function) {
    return static_cast<LibreSslSession*>(Connection(ssl, function).session);
}

const LibreSslCertKey* CurrentKey(const LibreSslSsl* ssl, const char* function) {
    const LibreSslCert* cert = static_cast<const LibreSslCert*>(Connection(ssl, function).cert);
    if (!cert) return nullptr;
    if (!cert->key) throw std::invalid_argument(std::string(function) + ": null current key");
    return cert->key;
}

}

extern "C" {

LibreSslSession* APS5_VABI SSL_get_session_nid_postfix(const LibreSslSsl* ssl) {
    return Session(ssl, __func__);
}

void APS5_VABI SSL_SESSION_free_nid_postfix(LibreSslSession* session) {
    if (!session) return;
    if (std::atomic_ref<std::int32_t>(session->references).fetch_sub(1) > 1) return;
    if (session->peer) NotImplemented_nid_no_patch("SSL_SESSION_free: peer certificate");
    if (session->ciphers) NotImplemented_nid_no_patch("SSL_SESSION_free: shared cipher list");
    if (session->internal) NotImplemented_nid_no_patch("SSL_SESSION_free: session internals");
    free_nid_postfix(session->tlsextHostname);
    free_nid_postfix(session->tlsextTick);
    free_nid_postfix(session);
}

int APS5_VABI SSL_set_session_nid_postfix(LibreSslSsl* ssl, LibreSslSession* session) {
    if (!ssl) APS5_INVALID_ARG_EX;
    if (!session) {
        SSL_SESSION_free_nid_postfix(static_cast<LibreSslSession*>(ssl->session));
        ssl->session = nullptr;
        return 1;
    }
    std::atomic_ref<std::int32_t>(session->references).fetch_add(1);
    SSL_SESSION_free_nid_postfix(static_cast<LibreSslSession*>(ssl->session));
    ssl->session = session;
    ssl->verifyResult = session->verifyResult;
    return 1;
}

const void* APS5_VABI SSL_get_current_cipher_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslSession* session = Session(ssl, __func__);
    return session ? session->cipher : nullptr;
}

void* APS5_VABI SSL_get_certificate_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslCertKey* key = CurrentKey(ssl, __func__);
    return key ? key->x509 : nullptr;
}

void* APS5_VABI SSL_get_privatekey_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslCertKey* key = CurrentKey(ssl, __func__);
    return key ? key->privatekey : nullptr;
}

}
