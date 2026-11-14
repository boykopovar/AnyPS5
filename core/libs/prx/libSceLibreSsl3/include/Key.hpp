#pragma once

#include <cstddef>
#include <cstdint>

struct LibreSsl3Bignum {
    std::uint64_t* data;
    std::int32_t top;
    std::int32_t capacity;
    std::int32_t negative;
    std::int32_t flags;
};

static_assert(sizeof(LibreSsl3Bignum) == 24);
static_assert(offsetof(LibreSsl3Bignum, flags) == 20);

struct LibreSsl3EvpPkey {
    std::int32_t type;
    std::int32_t references;
    const void* method;
    void* key;
    std::int32_t saveParameters;
};

struct LibreSsl3EcKey {
    const void* method;
    std::int32_t version;
    void* group;
    void* publicKey;
    void* privateKey;
    std::uint32_t encodingFlags;
    std::int32_t conversionForm;
    std::int32_t references;
    std::int32_t flags;
    void* extraData;
};

struct LibreSsl3Rsa {
    std::int32_t pad;
    std::int64_t version;
    const void* method;
    void* n;
    void* e;
    void* d;
    void* p;
    void* q;
    void* dmp1;
    void* dmq1;
    void* iqmp;
    void* pss;
    void* extraData;
    std::int32_t references;
    std::int32_t flags;
    void* methodModN;
    void* methodModP;
    void* methodModQ;
    void* blinding;
    void* mtBlinding;
};

struct LibreSsl3SslCtx {
    const void* method;
    const void* quicMethod;
    void* cipherList;
    void* certStore;
    std::int64_t sessionTimeout;
    std::int32_t references;
    void* extraCerts;
    std::int32_t verifyMode;
    std::uint64_t sidCtxLength;
    unsigned char sidCtx[32];
    void* param;
    void* defaultPasswdCallback;
    void* defaultPasswdCallbackUserdata;
    std::uint16_t minTlsVersion;
    std::uint16_t maxTlsVersion;
    std::uint16_t minProtoVersion;
    std::uint16_t maxProtoVersion;
    std::uint64_t options;
    std::uint64_t mode;
    void* newSessionCallback;
    void* removeSessionCallback;
    void* getSessionCallback;
    void* appVerifyCallback;
    void* appVerifyArgument;
    void* clientCertCallback;
    void* appGenerateCookieCallback;
    void* appVerifyCookieCallback;
    void* infoCallback;
    void* messageCallback;
    void* messageCallbackArgument;
    void* defaultVerifyCallback;
    void* generateSessionId;
    void* serverNameCallback;
    void* serverNameArgument;
    void* ticketKeyCallback;
    void* statusCallback;
    void* statusArgument;
    void* sessions;
    std::uint64_t sessionCacheSize;
    void* sessionCacheHead;
    void* sessionCacheTail;
    std::int32_t sessionCacheMode;
    std::int32_t stats[11];
    void* exData;
    void* cipherListTls13;
    void* cert;
    void* clientCa;
    std::int64_t maxCertList;
    std::int32_t readAhead;
    std::int32_t quietShutdown;
    std::uint32_t maxSendFragment;
    unsigned char ticketKeyName[16];
    unsigned char ticketHmacKey[16];
    unsigned char ticketAesKey[16];
    void* srtpProfiles;
    void* alpnSelectCallback;
    void* alpnSelectCallbackArgument;
    std::uint8_t* alpnClientProtoList;
    std::uint64_t alpnClientProtoListLength;
    std::uint64_t ecPointFormatListLength;
    std::uint8_t* ecPointFormatList;
    std::uint64_t supportedGroupsLength;
    std::uint16_t* supportedGroups;
    void* keylogCallback;
    std::uint64_t numTickets;
};

struct LibreSsl3Bio {
    const void* method;
    void* callback;
    void* callbackEx;
    char* callbackArgument;
    std::int32_t initialized;
    std::int32_t shutdown;
    std::int32_t flags;
    std::int32_t retryReason;
    std::int32_t number;
    void* data;
    LibreSsl3Bio* nextBio;
    LibreSsl3Bio* previousBio;
    std::int32_t references;
    std::uint64_t bytesRead;
    std::uint64_t bytesWritten;
    void* exData;
};

static_assert(sizeof(LibreSsl3EvpPkey) == 32);
static_assert(offsetof(LibreSsl3EvpPkey, references) == 4);
static_assert(sizeof(LibreSsl3EcKey) == 64);
static_assert(offsetof(LibreSsl3EcKey, references) == 48);
static_assert(sizeof(LibreSsl3Rsa) == 152);
static_assert(offsetof(LibreSsl3Rsa, references) == 104);
static_assert(sizeof(LibreSsl3SslCtx) == 568);
static_assert(offsetof(LibreSsl3SslCtx, references) == 40);
static_assert(offsetof(LibreSsl3SslCtx, minTlsVersion) == 128);
static_assert(offsetof(LibreSsl3SslCtx, maxTlsVersion) == 130);
static_assert(offsetof(LibreSsl3SslCtx, minProtoVersion) == 132);
static_assert(offsetof(LibreSsl3SslCtx, maxProtoVersion) == 134);
static_assert(offsetof(LibreSsl3SslCtx, infoCallback) == 216);
static_assert(offsetof(LibreSsl3SslCtx, exData) == 376);
static_assert(offsetof(LibreSsl3SslCtx, srtpProfiles) == 480);
static_assert(offsetof(LibreSsl3SslCtx, keylogCallback) == 552);
static_assert(offsetof(LibreSsl3Rsa, p) == 48);
static_assert(offsetof(LibreSsl3Rsa, q) == 56);
static_assert(offsetof(LibreSsl3Rsa, dmp1) == 64);
static_assert(offsetof(LibreSsl3Rsa, dmq1) == 72);
static_assert(offsetof(LibreSsl3Rsa, iqmp) == 80);
static_assert(sizeof(LibreSsl3Bio) == 112);
static_assert(offsetof(LibreSsl3Bio, references) == 80);
