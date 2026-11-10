#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "unicode/uclean.h"
#include <cstddef>
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

std::mutex NativeLock;
std::mutex MemoryLock;
MemoryFunctions Memory;
MemoryFunctions Snapshot() {
    const std::lock_guard lock(MemoryLock);
    return Memory;
}

void* U_CALLCONV Allocate(const void*, std::size_t size) {
    const auto memory = Snapshot();
    return memory.allocate(memory.context, size);
}

void* U_CALLCONV Reallocate(const void*, void* pointer, std::size_t size) {
    const auto memory = Snapshot();
    return memory.reallocate(memory.context, pointer, size);
}

void U_CALLCONV Release(const void*, void* pointer) {
    const auto memory = Snapshot();
    memory.release(memory.context, pointer);
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
    const std::lock_guard native(NativeLock);
    const std::lock_guard lock(MemoryLock);
    auto nativeStatus = static_cast<UErrorCode>(*status);
    u_setMemoryFunctions(nullptr, Allocate, Reallocate, Release, &nativeStatus);
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
    {
        const std::lock_guard native(NativeLock);
        u_cleanup();
        const std::lock_guard lock(MemoryLock);
        Memory = {};
    }
    const std::lock_guard lock(DirectoryLock);
    DataDirectory.clear();
    TimeZoneDirectory.clear();
}

}
