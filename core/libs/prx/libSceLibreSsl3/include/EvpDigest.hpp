#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL3_SRC_EVPDIGEST_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL3_SRC_EVPDIGEST_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace LibreSsl3 {

struct DigestContext;

struct DigestMethod {
    std::int32_t type;
    std::int32_t keyType;
    std::int32_t digestSize;
    std::uint64_t flags;
    int (APS5_VABI* init)(DigestContext*);
    int (APS5_VABI* update)(DigestContext*, const void*, std::size_t);
    int (APS5_VABI* finalize)(DigestContext*, unsigned char*);
    int (APS5_VABI* copy)(DigestContext*, const DigestContext*);
    int (APS5_VABI* cleanup)(DigestContext*);
    void* sign;
    void* verify;
    std::int32_t requiredKeyTypes[5];
    std::int32_t blockSize;
    std::int32_t contextSize;
    int (APS5_VABI* control)(DigestContext*, int, int, void*);
};

struct DigestContext {
    const DigestMethod* digest;
    void* engine;
    std::uint64_t flags;
    void* data;
    void* keyContext;
    int (APS5_VABI* update)(DigestContext*, const void*, std::size_t);
};

static_assert(sizeof(DigestMethod) == 120);
static_assert(offsetof(DigestMethod, init) == 24);
static_assert(offsetof(DigestMethod, flags) == 16);
static_assert(offsetof(DigestMethod, sign) == 64);
static_assert(offsetof(DigestMethod, verify) == 72);
static_assert(offsetof(DigestMethod, requiredKeyTypes) == 80);
static_assert(offsetof(DigestMethod, blockSize) == 100);
static_assert(offsetof(DigestMethod, contextSize) == 104);
static_assert(offsetof(DigestMethod, control) == 112);
static_assert(sizeof(DigestContext) == 48);
static_assert(offsetof(DigestContext, engine) == 8);
static_assert(offsetof(DigestContext, flags) == 16);
static_assert(offsetof(DigestContext, data) == 24);
static_assert(offsetof(DigestContext, keyContext) == 32);
static_assert(offsetof(DigestContext, update) == 40);

} // namespace LibreSsl3

#endif
