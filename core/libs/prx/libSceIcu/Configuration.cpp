// Derived from icu4c/source/common/cmemory.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_calloc below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cmemory.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2002-2015, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <cerrno>
#include <stdexcept>

namespace {

struct MemoryFunctions {
    const void* context = nullptr;
    Icu::MemoryAllocate allocate = nullptr;
    Icu::MemoryReallocate reallocate = nullptr;
    Icu::MemoryFree release = nullptr;
};

std::mutex MemoryLock;
MemoryFunctions Memory;
const std::int32_t ZeroMemory[] = {0, 0, 0, 0, 0, 0};

MemoryFunctions CurrentMemory() {
    const std::lock_guard lock(MemoryLock);
    return Memory;
}

void* Allocate(std::size_t size) {
    if (size == 0) return const_cast<std::int32_t*>(ZeroMemory);
    const MemoryFunctions functions = CurrentMemory();
    return functions.allocate ? functions.allocate(functions.context, size) : malloc_nid_postfix(size);
}

void Release(void* memory) {
    const MemoryFunctions functions = CurrentMemory();
    if (functions.release) functions.release(functions.context, memory);
    else free_nid_postfix(memory);
}
std::mutex DirectoryLock;
std::string DataDirectory;
std::string TimeZoneDirectory;

bool CopySource(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool SourceLength(const char* source, std::size_t& length) {
    char chunk[4096];
    for (std::size_t offset = 0;;) {
        const auto address = reinterpret_cast<std::uintptr_t>(source) + offset;
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        if (!CopySource(chunk, source + offset, count)) return false;
        if (const void* end = std::memchr(chunk, 0, count)) {
            length = offset + static_cast<std::size_t>(static_cast<const char*>(end) - chunk);
            return true;
        }
        offset += count;
    }
}

char* DuplicateSource(const char* source, std::size_t length, const char* function) {
    char* duplicate = static_cast<char*>(Allocate(length + 1));
    if (!duplicate) return nullptr;
    if (!CopySource(duplicate, source, length)) {
        Release(duplicate);
        throw std::invalid_argument(std::string(function) + ": unreadable source");
    }
    duplicate[length] = 0;
    return duplicate;
}

}

extern "C" {

void APS5_VABI u_setMutexFunctions_nid_postfix(const void*, const void*, const void*, const void*, const void*, std::int32_t* status) {
    if (!Icu::Failure(status)) *status = Icu::UnsupportedError;
}

void APS5_VABI u_setAtomicIncDecFunctions_nid_postfix(const void*, const void*, const void*, std::int32_t* status) {
    if (!Icu::Failure(status)) *status = Icu::UnsupportedError;
}


void APS5_VABI u_setMemoryFunctions_nid_postfix(const void* context, Icu::MemoryAllocate allocate, Icu::MemoryReallocate reallocate,
                                                Icu::MemoryFree release, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!allocate || !reallocate || !release) {
        *status = Icu::IllegalArgument;
        return;
    }
    const std::lock_guard lock(MemoryLock);
    Memory = {context, allocate, reallocate, release};
}


const char* APS5_VABI u_getDataDirectory_nid_postfix() {
    const std::lock_guard lock(DirectoryLock);
    return DataDirectory.c_str();
}

void APS5_VABI u_setDataDirectory_nid_postfix(const char* directory) {
    std::string copy = directory ? directory : "";
    const std::lock_guard lock(DirectoryLock);
    DataDirectory = std::move(copy);
}


const char* APS5_VABI u_getTimeZoneFilesDirectory_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return "";
    const std::lock_guard lock(DirectoryLock);
    return TimeZoneDirectory.c_str();
}

void APS5_VABI u_setTimeZoneFilesDirectory_nid_postfix(const char* path, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    const std::lock_guard lock(DirectoryLock);
    TimeZoneDirectory.clear();
    if (!path) {
        *status = Icu::IllegalArgument;
        return;
    }
    TimeZoneDirectory = path;
}


void APS5_VABI u_cleanup_nid_postfix() {
    {
        const std::lock_guard lock(MemoryLock);
        Memory = {};
    }
    const std::lock_guard lock(DirectoryLock);
    DataDirectory.clear();
    TimeZoneDirectory.clear();
}

void IcuReleaseMemory_nid_no_patch(void* memory) {
    if (memory != ZeroMemory) Release(memory);
}

void* APS5_VABI uprv_malloc_nid_postfix(std::size_t size) {
    return Allocate(size);
}

void* APS5_VABI uprv_realloc_nid_postfix(void* buffer, std::size_t size) {
    if (buffer == ZeroMemory) return Allocate(size);
    if (size == 0) {
        Release(buffer);
        return const_cast<std::int32_t*>(ZeroMemory);
    }
    const MemoryFunctions functions = CurrentMemory();
    return functions.reallocate ? functions.reallocate(functions.context, buffer, size) : realloc_nid_postfix(buffer, size);
}

void* APS5_VABI uprv_calloc_nid_postfix(std::size_t count, std::size_t size) {
    size *= count;
    void* memory = Allocate(size);
    if (memory) std::memset(memory, 0, size);
    return memory;
}

char* APS5_VABI uprv_strdup_nid_postfix(const char* source) {
    if (!source) APS5_INVALID_ARG_EX;
    std::size_t length = 0;
    if (!SourceLength(source, length)) throw std::invalid_argument(std::string(__func__) + ": unreadable source");
    return DuplicateSource(source, length, __func__);
}

char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t count) {
    if (count < 0) return uprv_strdup_nid_postfix(source);
    return DuplicateSource(source, static_cast<std::size_t>(count), __func__);
}

}
