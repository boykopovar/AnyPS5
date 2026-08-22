#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

extern "C" {

FILE* fopen_nid_postfix(const char* filename, const char* mode) {
    return std::fopen(filename, mode);
}

int fclose_nid_postfix(FILE* stream) {
    return std::fclose(stream);
}

size_t fread_nid_postfix(void* buffer, size_t size, size_t count, FILE* stream) {
    return std::fread(buffer, size, count, stream);
}

size_t fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FILE* stream) {
    return std::fwrite(buffer, size, count, stream);
}

int fseek_nid_postfix(FILE* stream, long offset, int origin) {
    return std::fseek(stream, offset, origin);
}

long ftell_nid_postfix(FILE* stream) {
    return std::ftell(stream);
}

int fputs_nid_postfix(const char* str, FILE* stream) {
    return std::fputs(str, stream);
}

int fflush_nid_postfix(FILE* stream) {
    return std::fflush(stream);
}

constexpr std::uintptr_t AlignedBlockTag = 1;

void* malloc_nid_postfix(size_t size) {
    const size_t headerSize = sizeof(std::uintptr_t);
    void* rawPtr = std::malloc(size + headerSize);
    if (rawPtr == nullptr) {
        throw std::bad_alloc();
    }
    *reinterpret_cast<std::uintptr_t*>(rawPtr) = 0;
    return static_cast<char*>(rawPtr) + headerSize;
}

void free_nid_postfix(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    const std::uintptr_t headerValue = reinterpret_cast<std::uintptr_t*>(ptr)[-1];
    if (headerValue == AlignedBlockTag) {
        std::free(reinterpret_cast<void**>(ptr)[-2]);
    } else {
        std::free(reinterpret_cast<char*>(ptr) - sizeof(std::uintptr_t));
    }
}

void* realloc_nid_postfix(void* ptr, size_t newSize) {
    if (ptr == nullptr) {
        return malloc_nid_postfix(newSize);
    }
    const size_t headerSize = sizeof(std::uintptr_t);
    void* rawPtr = static_cast<char*>(ptr) - headerSize;
    void* newRawPtr = std::realloc(rawPtr, newSize + headerSize);
    if (newRawPtr == nullptr) {
        throw std::bad_alloc();
    }
    return static_cast<char*>(newRawPtr) + headerSize;
}

void* memalign_nid_postfix(size_t alignment, size_t size) {
    const size_t headerSize = sizeof(void*) * 2;
    const size_t worstCaseSize = size + alignment + headerSize;
    void* rawPtr = std::malloc(worstCaseSize);
    if (rawPtr == nullptr) {
        throw std::bad_alloc();
    }
    const std::uintptr_t rawAddress = reinterpret_cast<std::uintptr_t>(rawPtr) + headerSize;
    const std::uintptr_t alignedAddress = (rawAddress + alignment - 1) & ~(alignment - 1);
    void* alignedPtr = reinterpret_cast<void*>(alignedAddress);
    reinterpret_cast<void**>(alignedPtr)[-1] = reinterpret_cast<void*>(AlignedBlockTag);
    reinterpret_cast<void**>(alignedPtr)[-2] = rawPtr;
    return alignedPtr;
}

void qsort_nid_postfix(void* base, size_t count, size_t size, int (*compare)(const void*, const void*)) {
    std::qsort(base, count, size, compare);
}

}
