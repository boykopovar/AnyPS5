#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCertKey) == 24);
static_assert(offsetof(LibreSslCertKey, privatekey) == 8);
static_assert(sizeof(LibreSslCert) == 256);
static_assert(sizeof(LibreSslSession) == 232);
static_assert(offsetof(LibreSslSession, verifyResult) == 136);
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

void Retain(LibreSslSession& session, const char* function) {
    auto references = std::atomic_ref<int>(session.references);
    int count = references.load(std::memory_order_relaxed);
    do {
        if (count == INT_MAX) throw std::overflow_error(std::string(function) + ": session reference count overflow");
    } while (!references.compare_exchange_weak(count, count + 1, std::memory_order_relaxed));
}

const LibreSslMethodInternal& MethodInternal(const void* method, const char* function) {
    const auto* value = static_cast<const LibreSslMethod*>(method);
    if (!value || !value->internal) throw std::invalid_argument(std::string(function) + ": null method");
    return *value->internal;
}

bool SetMethod(LibreSslSsl& ssl, const LibreSslMethod* method, const char* function) {
    if (ssl.method == method) return true;
    const LibreSslMethodInternal& current = MethodInternal(ssl.method, function);
    const LibreSslMethodInternal& next = MethodInternal(method, function);
    if (!ssl.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    void*& handshake = ssl.internal->handshakeFunc;
    const int connecting = handshake ? handshake == reinterpret_cast<void*>(current.sslConnect) : -1;
    bool created = true;
    if (current.version == next.version) {
        ssl.method = method;
    } else {
        current.sslFree(&ssl);
        ssl.method = method;
        created = next.sslNew(&ssl) != 0;
    }
    if (connecting == 1) handshake = reinterpret_cast<void*>(next.sslConnect);
    if (connecting == 0) handshake = reinterpret_cast<void*>(next.sslAccept);
    return created;
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
    if (session) Retain(*session, __func__);
    return session;
}

void APS5_VABI SSL_SESSION_free_nid_postfix(void* value) {
    if (!value) return;
    auto references = std::atomic_ref<int>(static_cast<LibreSslSession*>(value)->references);
    int count = references.load(std::memory_order_relaxed);
    do {
        if (count <= 0) throw std::invalid_argument(std::string(__func__) + ": session without references");
        if (count == 1) NotImplemented_nid_no_patch(__func__);
    } while (!references.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel, std::memory_order_relaxed));
}

int APS5_VABI SSL_set_session_nid_postfix(LibreSslSsl* ssl, void* value) {
    if (!ssl) throw std::invalid_argument(std::string(__func__) + ": null connection");
    const auto* ctx = static_cast<const LibreSslCtx*>(ssl->ctx);
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    auto* session = static_cast<LibreSslSession*>(value);
    auto* current = static_cast<LibreSslSession*>(ssl->session);
    if (!session) {
        SSL_SESSION_free_nid_postfix(current);
        ssl->session = nullptr;
        return SetMethod(*ssl, static_cast<const LibreSslMethod*>(ctx->method), __func__);
    }
    const LibreSslMethod* method = MethodInternal(ctx->method, __func__).getSslMethod(session->sslVersion);
    if (!method) method = MethodInternal(ssl->method, __func__).getSslMethod(session->sslVersion);
    if (!method) {
        LibreSslPutSslError_nid_no_patch(ssl, SslLayout::ReasonUnableToFindSslMethod);
        return 0;
    }
    if (!SetMethod(*ssl, method, __func__)) return 0;
    if (current != session) {
        Retain(*session, __func__);
        try {
            SSL_SESSION_free_nid_postfix(current);
        } catch (...) {
            std::atomic_ref<int>(session->references).fetch_sub(1);
            throw;
        }
    }
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
