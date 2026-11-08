#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uclean.h"
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

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

void* U_CALLCONV NativeAllocate(const void*, std::size_t size) {
    const MemoryFunctions functions = CurrentMemory();
    return functions.allocate(functions.context, size);
}

void* U_CALLCONV NativeReallocate(const void*, void* pointer, std::size_t size) {
    const MemoryFunctions functions = CurrentMemory();
    return functions.reallocate(functions.context, pointer, size);
}

void U_CALLCONV NativeRelease(const void*, void* pointer) {
    const MemoryFunctions functions = CurrentMemory();
    functions.release(functions.context, pointer);
}

std::mutex DirectoryLock;
std::string DataDirectory;
std::string TimeZoneDirectory;

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
    auto nativeStatus = static_cast<UErrorCode>(*status);
    u_setMemoryFunctions(nullptr, NativeAllocate, NativeReallocate, NativeRelease, &nativeStatus);
    *status = nativeStatus;
    if (U_SUCCESS(nativeStatus)) Memory = {context, allocate, reallocate, release};
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
    u_cleanup();
    {
        const std::lock_guard lock(MemoryLock);
        Memory = {};
    }
    const std::lock_guard lock(DirectoryLock);
    DataDirectory.clear();
    TimeZoneDirectory.clear();
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

char* APS5_VABI uprv_strdup_nid_postfix(const char* source) {
    if (!source) APS5_INVALID_ARG_EX;
    const std::size_t length = std::strlen(source) + 1;
    char* duplicate = static_cast<char*>(Allocate(length));
    if (duplicate) std::memcpy(duplicate, source, length);
    return duplicate;
}

char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t count) {
    if (count < 0) return uprv_strdup_nid_postfix(source);
    const std::size_t length = static_cast<std::size_t>(count);
    char* duplicate = static_cast<char*>(Allocate(length + 1));
    if (duplicate) {
        if (length != 0) std::memcpy(duplicate, source, length);
        duplicate[length] = 0;
    }
    return duplicate;
}

}
