#include "prx/libSceLibreSsl/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCertKey) == 24);
static_assert(offsetof(LibreSslCertKey, privatekey) == 8);
static_assert(sizeof(LibreSslCert) == 256);
static_assert(sizeof(LibreSslSession) == 232);
static_assert(offsetof(LibreSslSession, references) == 160);
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

void* APS5_VABI SSL_get1_session_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSession* session = Session(ssl, __func__);
    if (session) std::atomic_ref<int>(session->references).fetch_add(1);
    return session;
}

void APS5_VABI SSL_SESSION_free_nid_postfix(void* value) {
    if (!value) return;
    auto references = std::atomic_ref<int>(static_cast<LibreSslSession*>(value)->references);
    if (references.fetch_sub(1, std::memory_order_acq_rel) - 1 > 0) return;
    references.fetch_add(1, std::memory_order_relaxed);
    NotImplemented_nid_no_patch(__func__);
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
