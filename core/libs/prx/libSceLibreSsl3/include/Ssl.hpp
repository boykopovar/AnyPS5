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
    std::int32_t clientVersion;
    std::uint32_t maxSendFragment;
    const void* const* tlsextBuildOrder;
    std::uint64_t tlsextBuildOrderLength;
    char* tlsextHostname;
    std::int32_t tlsextStatusType;
    LibreSsl3SslCtx* initialCtx;
    void* tls13;
    std::uint16_t minTlsVersion;
    std::uint16_t maxTlsVersion;
    std::uint16_t minProtoVersion;
    std::uint16_t maxProtoVersion;
    std::uint64_t options;
    std::uint64_t mode;
    std::uint8_t* alpnClientProtoList;
    std::uint64_t alpnClientProtoListLength;
    std::uint8_t* quicTransportParams;
    std::uint64_t quicTransportParamsLength;
    std::int32_t inHandshake;
    void* handshakeFunction;
    void* infoCallback;
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
static_assert(sizeof(LibreSsl3Ssl) == 320);
static_assert(offsetof(LibreSsl3Ssl, s3) == 56);
static_assert(offsetof(LibreSsl3Ssl, references) == 176);
static_assert(offsetof(LibreSsl3Ssl, initialCtx) == 224);
static_assert(offsetof(LibreSsl3Ssl, minTlsVersion) == 240);
static_assert(offsetof(LibreSsl3Ssl, quicTransportParams) == 280);
static_assert(offsetof(LibreSsl3Ssl, infoCallback) == 312);
static_assert(offsetof(LibreSsl3SslSession, masterKeyLength) == 8);
static_assert(offsetof(LibreSsl3SslSession, masterKey) == 16);
static_assert(sizeof(LibreSsl3SslSession) == 192);
static_assert(offsetof(LibreSsl3SslSession, references) == 184);
