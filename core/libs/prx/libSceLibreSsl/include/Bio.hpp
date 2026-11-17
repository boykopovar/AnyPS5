#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_BIO_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_BIO_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace LibreSsl {

struct Bio;
using BioCallback = std::int64_t (APS5_VABI *)(Bio*, int, const char*, int, std::int64_t, std::int64_t);
using BioInfoCallback = void (APS5_VABI *)(Bio*, int, const char*, int, std::int64_t, std::int64_t);

struct BioMethod {
    int type;
    const char* name;
    int (APS5_VABI *write)(Bio*, const char*, int);
    int (APS5_VABI *read)(Bio*, char*, int);
    int (APS5_VABI *puts)(Bio*, const char*);
    int (APS5_VABI *gets)(Bio*, char*, int);
    std::int64_t (APS5_VABI *control)(Bio*, int, std::int64_t, void*);
    int (APS5_VABI *create)(Bio*);
    int (APS5_VABI *destroy)(Bio*);
    std::int64_t (APS5_VABI *callbackControl)(Bio*, int, BioInfoCallback);
};

struct Bio {
    const BioMethod* method;
    BioCallback callback;
    char* callbackArgument;
    int initialized;
    int shutdown;
    int flags;
    int retryReason;
    int number;
    void* data;
    Bio* next;
    Bio* previous;
    int references;
    std::uint64_t bytesRead;
    std::uint64_t bytesWritten;
    void* extraData;
};

struct BioBuffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

static_assert(sizeof(BioMethod) == 80);
static_assert(offsetof(BioMethod, write) == 16);
static_assert(offsetof(BioMethod, control) == 48);
static_assert(offsetof(BioMethod, callbackControl) == 72);
static_assert(sizeof(Bio) == 104);
static_assert(offsetof(Bio, initialized) == 24);
static_assert(offsetof(Bio, flags) == 32);
static_assert(offsetof(Bio, number) == 40);
static_assert(offsetof(Bio, data) == 48);
static_assert(offsetof(Bio, references) == 72);
static_assert(offsetof(Bio, bytesRead) == 80);
static_assert(offsetof(Bio, bytesWritten) == 88);
static_assert(offsetof(Bio, extraData) == 96);
static_assert(sizeof(BioBuffer) == 24);
static_assert(offsetof(BioBuffer, data) == 8);
static_assert(offsetof(BioBuffer, capacity) == 16);

}  // namespace LibreSsl

extern "C" int LibreSslBioWrite_nid_no_patch(LibreSsl::Bio* bio, const void* source, int size);

#endif
