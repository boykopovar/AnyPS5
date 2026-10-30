#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl3/include/Key.hpp"

struct LibreSsl3SslMethod {
    std::int32_t dtls;
    std::int32_t server;
    std::int32_t version;
    std::uint16_t minTlsVersion;
    std::uint16_t maxTlsVersion;
};

struct LibreSsl3Ssl3State {
    std::int64_t flags;
    unsigned char serverRandom[32];
    unsigned char clientRandom[32];
};

struct LibreSsl3Ssl {
    std::int32_t version;
    const LibreSsl3SslMethod* method;
    const void* quicMethod;
    void* rbio;
    void* wbio;
    void* bbio;
    std::int32_t server;
    LibreSsl3Ssl3State* s3;
    void* d1;
    void* param;
    void* cipherList;
    void* cert;
    std::uint64_t sidCtxLength;
    unsigned char sidCtx[32];
    void* session;
    std::int32_t verifyMode;
    std::int32_t error;
    std::int32_t errorCode;
    LibreSsl3SslCtx* ctx;
    std::int64_t verifyResult;
    std::int32_t references;
};

struct LibreSsl3SslSession {
    std::int32_t sslVersion;
    std::uint64_t masterKeyLength;
    unsigned char masterKey[48];
    std::uint64_t sessionIdLength;
    unsigned char sessionId[32];
    std::uint64_t sidCtxLength;
    unsigned char sidCtx[32];
    void* peerCert;
    std::int32_t peerCertType;
    std::int64_t verifyResult;
    std::int64_t timeout;
    std::int64_t time;
    std::int32_t references;
};

static_assert(sizeof(LibreSsl3SslMethod) == 16);
static_assert(offsetof(LibreSsl3SslMethod, minTlsVersion) == 12);
static_assert(offsetof(LibreSsl3SslMethod, maxTlsVersion) == 14);
static_assert(sizeof(LibreSsl3Ssl3State) == 72);
static_assert(offsetof(LibreSsl3Ssl3State, clientRandom) == 40);
static_assert(sizeof(LibreSsl3Ssl) == 184);
static_assert(offsetof(LibreSsl3Ssl, s3) == 56);
static_assert(offsetof(LibreSsl3Ssl, references) == 176);
static_assert(sizeof(LibreSsl3SslSession) == 192);
static_assert(offsetof(LibreSsl3SslSession, references) == 184);
