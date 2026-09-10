#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "prx/libc/include/FileStream.hpp"


extern "C" {

[[noreturn]] void _ZSt11_Xbad_allocv_nid_postfix();

FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode) {
    if (!filename || !mode) throw std::runtime_error("fopen: null argument");
    std::unique_ptr<std::FILE, decltype(&std::fclose)> handle(std::fopen(filename, mode), std::fclose);
    if (!handle) {
        std::string msg = "fopen: open failed: \"";
        msg += filename;
        msg += "\": ";
        msg += std::strerror(errno);
        throw std::runtime_error(msg);
    }
    auto stream = std::make_unique<FileStream>(handle.get(), true);
    handle.release();

    APS5_LOG_OUT("success: \"%s\"", filename);
    return stream.release();
}

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    GetNativeStream(stream);
    std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
    stream->Close();
    return 0;
}

size_t APS5_VABI fread_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fread: null buffer");
    const auto result = std::fread(buffer, size, count, handle);
    if (std::ferror(handle)) throw std::runtime_error("fread: read failed");
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
    const auto result = std::fwrite(buffer, size, count, handle);
    if (result != count || std::ferror(handle)) throw std::runtime_error("fwrite: write failed");
    return result;
}

int APS5_VABI fseek_nid_postfix(FileStream* stream, long offset, int origin) {
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) throw std::runtime_error("fseek: invalid origin");
    if (std::fseek(GetNativeStream(stream), offset, origin) != 0) throw std::runtime_error("fseek: seek failed");
    return 0;
}

long APS5_VABI ftell_nid_postfix(FileStream* stream) {
    const auto result = std::ftell(GetNativeStream(stream));
    if (result == -1L) throw std::runtime_error("ftell: position query failed");
    return result;
}

int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream) {
    if (!str) throw std::runtime_error("fputs: null string");
    const int result = std::fputs(str, GetNativeStream(stream));
    if (result == EOF) throw std::runtime_error("fputs: write failed");
    return result;
}

int APS5_VABI fflush_nid_postfix(FileStream* stream) {
    if (std::fflush(stream ? GetNativeStream(stream) : nullptr) != 0) throw std::runtime_error("fflush: flush failed");
    return 0;
}

constexpr std::uintptr_t AlignedBlockTag = 1;

void* APS5_VABI malloc_nid_postfix(size_t size) {
    const size_t headerSize = sizeof(std::uintptr_t);
    void* rawPtr = std::malloc(size + headerSize);
    if (rawPtr == nullptr) {
        _ZSt11_Xbad_allocv_nid_postfix();
    }
    *reinterpret_cast<std::uintptr_t*>(rawPtr) = 0;
    return static_cast<char*>(rawPtr) + headerSize;
}

void APS5_VABI free_nid_postfix(void* ptr) {
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

void* APS5_VABI realloc_nid_postfix(void* ptr, size_t newSize) {
    if (ptr == nullptr) {
        return malloc_nid_postfix(newSize);
    }
    const size_t headerSize = sizeof(std::uintptr_t);
    void* rawPtr = static_cast<char*>(ptr) - headerSize;
    void* newRawPtr = std::realloc(rawPtr, newSize + headerSize);
    if (newRawPtr == nullptr) {
        _ZSt11_Xbad_allocv_nid_postfix();
    }
    return static_cast<char*>(newRawPtr) + headerSize;
}

void* APS5_VABI memalign_nid_postfix(size_t alignment, size_t size) {
    const size_t headerSize = sizeof(void*) * 2;
    const size_t worstCaseSize = size + alignment + headerSize;
    void* rawPtr = std::malloc(worstCaseSize);
    if (rawPtr == nullptr) {
        _ZSt11_Xbad_allocv_nid_postfix();
    }
    const std::uintptr_t rawAddress = reinterpret_cast<std::uintptr_t>(rawPtr) + headerSize;
    const std::uintptr_t alignedAddress = (rawAddress + alignment - 1) & ~(alignment - 1);
    void* alignedPtr = reinterpret_cast<void*>(alignedAddress);
    reinterpret_cast<void**>(alignedPtr)[-1] = reinterpret_cast<void*>(AlignedBlockTag);
    reinterpret_cast<void**>(alignedPtr)[-2] = rawPtr;
    return alignedPtr;
}

void APS5_VABI qsort_nid_postfix(void* base, size_t count, size_t size, int (*compare)(const void*, const void*)) {
    std::qsort(base, count, size, compare);
}

}
