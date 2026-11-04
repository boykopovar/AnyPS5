#pragma once

#include <cstddef>
#include <cstdint>

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

static_assert(sizeof(LibreSsl3EvpPkey) == 32);
static_assert(offsetof(LibreSsl3EvpPkey, references) == 4);
static_assert(sizeof(LibreSsl3EcKey) == 64);
static_assert(offsetof(LibreSsl3EcKey, references) == 48);
static_assert(sizeof(LibreSsl3Rsa) == 152);
static_assert(offsetof(LibreSsl3Rsa, references) == 104);
