#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct LibreSsl3Bio;

using LibreSsl3BioWrite = int (APS5_VABI*)(LibreSsl3Bio*, const char*, int);
using LibreSsl3BioRead = int (APS5_VABI*)(LibreSsl3Bio*, char*, int);
using LibreSsl3BioPuts = int (APS5_VABI*)(LibreSsl3Bio*, const char*);
using LibreSsl3BioControl = std::int64_t (APS5_VABI*)(LibreSsl3Bio*, int, std::int64_t, void*);
using LibreSsl3BioLifecycle = int (APS5_VABI*)(LibreSsl3Bio*);

struct LibreSsl3BioMethod {
    std::int32_t type;
    const char* name;
    LibreSsl3BioWrite write;
    LibreSsl3BioRead read;
    LibreSsl3BioPuts puts;
    LibreSsl3BioRead gets;
    LibreSsl3BioControl control;
    LibreSsl3BioLifecycle create;
    LibreSsl3BioLifecycle destroy;
    void* callbackControl;
};

struct LibreSsl3Bio {
    const LibreSsl3BioMethod* method;
    void* callback;
    void* callbackEx;
    char* callbackArgument;
    std::int32_t initialized;
    std::int32_t shutdown;
    std::int32_t flags;
    std::int32_t retryReason;
    std::int32_t number;
    void* data;
    LibreSsl3Bio* next;
    LibreSsl3Bio* previous;
    std::int32_t references;
    std::uint64_t bytesRead;
    std::uint64_t bytesWritten;
    void* extraData;
};

static_assert(sizeof(LibreSsl3BioMethod) == 80);
static_assert(offsetof(LibreSsl3BioMethod, name) == 8);
static_assert(offsetof(LibreSsl3BioMethod, write) == 16);
static_assert(offsetof(LibreSsl3BioMethod, control) == 48);
static_assert(offsetof(LibreSsl3BioMethod, create) == 56);
static_assert(offsetof(LibreSsl3BioMethod, destroy) == 64);
static_assert(sizeof(LibreSsl3Bio) == 112);
static_assert(offsetof(LibreSsl3Bio, initialized) == 32);
static_assert(offsetof(LibreSsl3Bio, shutdown) == 36);
static_assert(offsetof(LibreSsl3Bio, flags) == 40);
static_assert(offsetof(LibreSsl3Bio, data) == 56);
static_assert(offsetof(LibreSsl3Bio, extraData) == 104);
