#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSsl3Rsa {
    int pad;
    std::int64_t version;
    const void* meth;
    const void* n;
    const void* e;
    const void* d;
    const void* p;
    const void* q;
    const void* dmp1;
    const void* dmq1;
    const void* iqmp;
    unsigned char rest[64];
};

struct LibreSsl3Dh {
    int pad;
    int version;
    const void* p;
    const void* g;
    std::int64_t length;
    const void* pubKey;
    const void* privKey;
    int flags;
    void* methodMontP;
    const void* q;
    unsigned char rest[56];
};

struct LibreSsl3Dsa {
    int pad;
    std::int64_t version;
    const void* p;
    const void* q;
    const void* g;
    const void* pubKey;
    const void* privKey;
    unsigned char rest[56];
};

static_assert(sizeof(LibreSsl3Rsa) == 152);
static_assert(offsetof(LibreSsl3Rsa, n) == 24);
static_assert(offsetof(LibreSsl3Rsa, d) == 40);
static_assert(sizeof(LibreSsl3Dh) == 128);
static_assert(offsetof(LibreSsl3Dh, p) == 8);
static_assert(offsetof(LibreSsl3Dh, g) == 16);
static_assert(offsetof(LibreSsl3Dh, pubKey) == 32);
static_assert(offsetof(LibreSsl3Dh, privKey) == 40);
static_assert(offsetof(LibreSsl3Dh, q) == 64);
static_assert(sizeof(LibreSsl3Dsa) == 112);
static_assert(offsetof(LibreSsl3Dsa, p) == 16);
static_assert(offsetof(LibreSsl3Dsa, g) == 32);
static_assert(offsetof(LibreSsl3Dsa, privKey) == 48);

namespace {

template<typename TKey> const TKey& Key(const TKey* key, const char* function) {
    if (!key) throw std::invalid_argument(std::string(function) + ": null key");
    return *key;
}

void Store(const void** out, const void* value) {
    if (out) *out = value;
}

}

extern "C" {

void APS5_VABI RSA_get0_key_nid_postfix(const LibreSsl3Rsa* rsa, const void** n, const void** e, const void** d) {
    const LibreSsl3Rsa& key = Key(rsa, __func__);
    Store(n, key.n);
    Store(e, key.e);
    Store(d, key.d);
}

void APS5_VABI DH_get0_pqg_nid_postfix(const LibreSsl3Dh* dh, const void** p, const void** q, const void** g) {
    const LibreSsl3Dh& key = Key(dh, __func__);
    Store(p, key.p);
    Store(q, key.q);
    Store(g, key.g);
}

void APS5_VABI DH_get0_key_nid_postfix(const LibreSsl3Dh* dh, const void** pubKey, const void** privKey) {
    const LibreSsl3Dh& key = Key(dh, __func__);
    Store(pubKey, key.pubKey);
    Store(privKey, key.privKey);
}

void APS5_VABI DSA_get0_pqg_nid_postfix(const LibreSsl3Dsa* dsa, const void** p, const void** q, const void** g) {
    const LibreSsl3Dsa& key = Key(dsa, __func__);
    Store(p, key.p);
    Store(q, key.q);
    Store(g, key.g);
}

}
