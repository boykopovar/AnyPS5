#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
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
std::mutex DirectoryLock;
std::string DataDirectory;
std::string TimeZoneDirectory;

alignas(std::max_align_t) unsigned char EmptyBlock[sizeof(std::max_align_t)];

MemoryFunctions CurrentMemory() {
    const std::lock_guard lock(MemoryLock);
    return Memory;
}

void* Allocate(std::size_t size) {
    if (size == 0) return EmptyBlock;
    const auto memory = CurrentMemory();
    return memory.allocate ? memory.allocate(memory.context, size) : malloc_nid_postfix(size);
}

char* Duplicate(const char* source, std::size_t length) {
    auto* copy = static_cast<char*>(Allocate(length + 1));
    if (!copy) return nullptr;
    std::memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
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

void* APS5_VABI uprv_malloc_nid_postfix(std::size_t size) {
    return Allocate(size);
}

void* APS5_VABI uprv_realloc_nid_postfix(void* block, std::size_t size) {
    if (block == EmptyBlock) return Allocate(size);
    const auto memory = CurrentMemory();
    if (size == 0) {
        if (memory.release) memory.release(memory.context, block);
        else free_nid_postfix(block);
        return EmptyBlock;
    }
    return memory.reallocate ? memory.reallocate(memory.context, block, size) : realloc_nid_postfix(block, size);
}

char* APS5_VABI uprv_strdup_nid_postfix(const char* source) {
    if (!source) APS5_INVALID_ARG_EX;
    return Duplicate(source, std::strlen(source));
}

char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t length) {
    if (!source) APS5_INVALID_ARG_EX;
    if (length < 0) return Duplicate(source, std::strlen(source));
    return Duplicate(source, static_cast<std::size_t>(length));
}

}
