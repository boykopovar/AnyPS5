#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslSslInternal {
    unsigned char beforeRwstate[156];
    int rwstate;
    unsigned char beforeShutdown[8];
    int shutdown;
    unsigned char rest[260];
};

struct LibreSsl {
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
    void* ctx;
    std::int64_t verifyResult;
    int references;
    int clientVersion;
    unsigned int maxSendFragment;
    char* tlsextHostname;
    int tlsextStatusType;
    void* initialCtx;
    void* encReadCtx;
    void* readHash;
    LibreSslSslInternal* internal;
};

static_assert(sizeof(LibreSslSslInternal) == 432);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(sizeof(LibreSsl) == 232);
static_assert(offsetof(LibreSsl, rbio) == 16);
static_assert(offsetof(LibreSsl, wbio) == 24);
static_assert(offsetof(LibreSsl, session) == 128);
static_assert(offsetof(LibreSsl, ctx) == 152);
static_assert(offsetof(LibreSsl, verifyResult) == 160);
static_assert(offsetof(LibreSsl, internal) == 224);

namespace {

const LibreSsl& Connection(const LibreSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslSslInternal& Internal(const LibreSsl* ssl, const char* function) {
    const LibreSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

}

extern "C" {

void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).ctx;
}

}
